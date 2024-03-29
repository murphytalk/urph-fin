#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#include <pwd.h>
#endif  // _WIN32

#if defined(__ANDROID__)
#include <android/native_activity.h>
#include <jni.h>

// Get the JNI environment.
JNIEnv* GetJniEnv();
// Get the activity.
jobject GetActivity();

#elif defined(__APPLE__)
extern "C" {
#include <objc/objc.h>
}  // extern "C"
#endif  // __ANDROID__



#ifdef _WIN32
#include <windows.h>
#endif  // _WIN32

#include <cassert>
#include <memory>
#include <stdexcept>
#include <sstream> 
#include <string>
#include <functional>
#include <vector>
#include <algorithm>
#include <ctime>

#include "../utils.hxx"

#include "../core/urph-fin-core.hxx"
#include "storage.hxx"

#include "firebase/auth.h"
#include "firebase/auth.h"
#include "firebase/auth/credential.h"
#include "firebase/util.h"
#include "firebase/firestore.h"

#include "yaml-cpp/yaml.h"
using ::firebase::App;
using ::firebase::AppOptions;
using ::firebase::Future;
using ::firebase::FutureBase;
using ::firebase::Variant;
using ::firebase::Timestamp;
using ::firebase::auth::AdditionalUserInfo;
using ::firebase::auth::Auth;
using ::firebase::auth::User;
using ::firebase::auth::AuthError;
using ::firebase::auth::Credential;
using ::firebase::auth::EmailAuthProvider;

using ::firebase::firestore::Firestore;
using ::firebase::firestore::CollectionReference;
using ::firebase::firestore::DocumentReference;
using ::firebase::firestore::DocumentSnapshot;
using ::firebase::firestore::FieldValue;
using ::firebase::firestore::FieldPath;
using ::firebase::firestore::Query;
using ::firebase::firestore::QuerySnapshot;
using ::firebase::firestore::Error;

#ifdef _WIN32
#include <processenv.h>
#endif

static std::string get_home_dir()
{
    const char *homedir;
#ifdef _WIN32
    //HOMEPATH or user profile
    char buf[128];
    ExpandEnvironmentStringsA("%HOMEDRIVE%%HOMEPATH%",buf, sizeof(buf));
    homedir = buf;
#else
    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }
#endif
    // compiler is smart enough to either do return-value-optimization or use move
    //https://stackoverflow.com/questions/4986673/c11-rvalues-and-move-semantics-confusion-return-statement
    return std::string(homedir);
}

Config load_cfg(){
    try {
        auto cfg = YAML::LoadFile(get_home_dir() + "/.finance-credentials/urph-fin.yaml");
        auto userCfg = cfg["user"];
        return Config{ userCfg["email"].as<std::string>(), userCfg["password"].as<std::string>() };
    }
    catch (const std::runtime_error& err) {
        LOG(ERROR) << "Failed to load config file:" << err.what();
        return Config();
    }
}

static bool quit = false;

static bool ProcessEvents(int msec) {
#ifdef _WIN32
  Sleep(msec);
#else
  usleep(msec * 1000);
#endif  // _WIN32
  return quit;
}

static const int kTimeoutMs = 5000;
static const int kSleepMs = 100;
// Waits for a Future to be completed and returns whether the future has
// completed successfully. If the Future returns an error, it will be logged.
static bool Await(const firebase::FutureBase& future, const char* name) {
  int remaining_timeout = kTimeoutMs;
  while (future.status() == firebase::kFutureStatusPending /*&& remaining_timeout > 0*/) {
    remaining_timeout -= kSleepMs;
    ProcessEvents(kSleepMs);
  }

  if (future.status() != firebase::kFutureStatusComplete) {
    LOG(ERROR) << name << " returned an invalid status " << future.status() << "\n";
    return false;
  } else if (future.error() != 0) {
    LOG(ERROR) << name << "returned error code=" << future.error() <<",msg=" << future.error_message() << "\n";
    return false;
  }
  return true;
}

class FirestoreDao{
private:
    App*  _firebaseApp;
    Auth* _firebaseAuth;
    Firestore* _firestore;

    static const char COLLECTION_BROKERS [];
    static const char COLLECTION_INSTRUMENTS[];
    static const char COLLECTION_TX[];
    static const char COLLECTION_QUOTES[];
    static const int FILTER_WHERE_IN_LIMIT = 10; // Firestore API constrain
public:
    using BrokerType = DocumentSnapshot;
    FirestoreDao(OnDone onInitDone){
        _firebaseApp = 
#if defined(__ANDROID__)
            App::Create(GetJniEnv(), GetActivity())
#else
            ::firebase::App::Create()
#endif
        ;

        _firestore = nullptr;
        char buf[200];
        LOG(INFO) << "Started at " << getcwd(buf, 200)<< "\n";
        void* initialize_targets[] = {&_firestore};

        const firebase::ModuleInitializer::InitializerFn initializers[] = {
            // auth
            [](::firebase::App* app, void*) {
                LOG(INFO)<<"Attempt to initialize Firebase Auth.\n";
                ::firebase::InitResult init_result;
                Auth::GetAuth(app, &init_result);
                return init_result;
            },
            // firestore
            [](firebase::App* app, void* data) {
                LOG(INFO)<<"Attempt to initialize Firebase Firestore.\n";
                void** targets = reinterpret_cast<void**>(data);
                firebase::InitResult result;
                *reinterpret_cast<Firestore**>(targets[0]) = Firestore::GetInstance(app, &result);
                return result;
            }
        };

        ::firebase::ModuleInitializer initializer;
        initializer.Initialize(_firebaseApp, initialize_targets, initializers, sizeof(initializers) / sizeof(initializers[0]));

        while (initializer.InitializeLastResult().status() != firebase::kFutureStatusComplete) {
            if (ProcessEvents(100)) throw std::runtime_error("Timeout when wait for firestore app to initialize");
        }

        if (initializer.InitializeLastResult().error() != 0) {
            if (ProcessEvents(200)) throw std::runtime_error("Failed to ini firestore app");
        }

        _firebaseAuth = Auth::GetAuth(_firebaseApp);

        LOG(DEBUG) << "Cloud instance created\n";

        auto c = [](const Future<User*>& u, void* onInitDone){
            auto err_code = u.error();
            if(err_code == 0){
                LOG(INFO) << "Auth completed\n";
                (*(OnDone)onInitDone)(nullptr);
            }
            else{
                std::ostringstream ss;
                ss << "Failed to auth : err code - " << err_code << ", err msg - " <<  u.error_message();
                throw std::runtime_error(ss.str());
            }

        };
        auto cfg = load_cfg();
        LOG(INFO) << "Log in as " << cfg.email << "\n"; 
        _firebaseAuth->SignInWithEmailAndPassword(cfg.email.c_str(), cfg.password.c_str())
        .OnCompletion(c, (void*)onInitDone);
        LOG(DEBUG) << "FirestoreDao created\n";
    }
    ~FirestoreDao(){
        _firebaseAuth->SignOut();
        delete _firestore;
        delete _firebaseAuth;
        delete _firebaseApp;
        LOG(DEBUG) << "FirestoreDao destroyed\n";
    }

    BrokerType get_broker_by_name(const char* name){
        const auto& doc_ref = _firestore->Collection(COLLECTION_BROKERS).Document(name);
        const auto& f = doc_ref.Get();
        if(Await(f, "broker by name")){
            return *f.result();
        }
        else throw std::runtime_error("Cannot get broker");
    }

    std::string get_broker_name(const BrokerType& broker){
        return broker.id();
    }

    BrokerBuilder get_broker_cash_balance_and_active_funds(const BrokerType& broker)
    {
        const auto& cash = broker.Get("cash");
        const auto& all_ccys = cash.map_value();
        LOG(DEBUG) << " " << broker.id() << "\n";

        const auto& active_funds = broker.Get("active_funds").array_value();
        BrokerBuilder f(all_ccys.size(),active_funds.size());

        for (const auto& b : all_ccys) {
            LOG(DEBUG) << "  " << b.first << " " << b.second << "" "\n";
            f.add_cash_balance(b.first, FirestoreDao::get_num_as_double(b.second));
        }

        for(const auto& a: active_funds){
            f.add_active_fund(a.string_value());
        }

        return f;
    }


    typedef AllBrokerBuilder<FirestoreDao, FirestoreDao::BrokerType> AllBrokerBuilderType;
    AllBrokerBuilderType* get_brokers()
    {
        return for_each_broker<AllBrokerBuilderType>([&](const std::vector<DocumentSnapshot>& all_brokers) -> AllBrokerBuilderType*{
            auto *p = new AllBrokerBuilder<FirestoreDao, FirestoreDao::BrokerType>(all_brokers.size());
            LOG(DEBUG) << "Loading " << all_brokers.size() << " brokers\n";
            for (const auto& broker : all_brokers) {
                p->add_broker(this, broker);
            }
            return p;
        });
    }

    StringsBuilder* get_all_broker_names()
    {
        return for_each_broker<StringsBuilder>([&](const std::vector<DocumentSnapshot>& all_brokers) -> StringsBuilder*{
            StringsBuilder* p = new StringsBuilder(all_brokers.size());
            for (size_t i = 0; i < all_brokers.size(); ++i){
                auto broker_name = all_brokers[i].id();
                p->add(broker_name);
            }
            return p;
        });
    }

    void get_funds(FundsBuilder* builder, int funds_num, const char **fund_ids_head)
    {
        // cannot use auto var allocated in stack, as it will be freed once out of the context of OnCompletion()
        // and becomes invalid when the lambda is called
        auto q = _firestore->Collection(COLLECTION_INSTRUMENTS);
        filter_where_in(q, fund_ids_head, funds_num,[builder](const Future<QuerySnapshot>& future) {
            FirestoreDao::sub_collection(builder, future, FirestoreDao::COLLECTION_TX,
                [](const Query& q){ return q.OrderBy("date", Query::Direction::kDescending).Limit(1); },
                [builder](const std::vector<DocumentSnapshot>& docs){
                    const auto& tx_ref = docs.front();
                    const auto& broker = tx_ref.Get("broker").string_value();
                    const auto& name = tx_ref.Get("instrument_name").string_value();
                    const auto& id   = tx_ref.Get("instrument_id").string_value();
                    const auto amt = tx_ref.Get("amount").integer_value();
                    double capital = FirestoreDao::get_num_as_double(tx_ref.Get("capital"));
                    double market_value = FirestoreDao::get_num_as_double(tx_ref.Get("market_value"));
                    double price = FirestoreDao::get_num_as_double(tx_ref.Get("price"));
                    double profit = market_value - capital;
                    double roi = profit / capital;
                    timestamp date = tx_ref.Get("date").timestamp_value().seconds();
                    LOG(DEBUG) << "got tx of broker " << broker << ".fund id="<<id<<",name="<<name << "\n";
                    builder->add_fund(
                        broker, name, id,
                        (int)amt,
                        capital,
                        market_value,
                        price,
                        profit,
                        roi,
                        date
                    );
                    LOG(DEBUG) << "No." << builder->alloc->allocated_num() << " created: tx of broker " << broker << ".fund id="<<id<<",name="<<name << "\n";
                });
        });
    }

    void get_stock_portfolio(StockPortfolioBuilder* builder, const char* broker, const char* symbol)
    {
        get_stocks(symbol).OnCompletion([builder, broker](const Future<QuerySnapshot>& future){
            if(future.error() == Error::kErrorOk){
                const auto& stocks = future.result()->documents();
                builder->prepare_stock_alloc(stocks.size());
                for(const auto& stock: stocks){
                    const auto& my_symbol = stock.id();
                    const auto& ccy = stock.Get("ccy").string_value();
                    builder->add_stock(my_symbol, ccy);
                    const Query& q1 = stock.reference().Collection(FirestoreDao::COLLECTION_TX);
                    const auto& q2 = broker == nullptr ? q1 : q1.WhereEqualTo("broker", FieldValue::String(broker));
                    const auto& qs = q2.Get();
                    qs.OnCompletion([builder, &my_symbol](const Future<QuerySnapshot>& future){
                        if(future.error() == Error::kErrorOk){
                            const auto& txx = future.result()->documents();
                            builder->prepare_tx_alloc(my_symbol, txx.size());
                            for(const auto& tx: txx){
                                builder->incr_counter(my_symbol);
                                const timestamp date = tx.Get("date").timestamp_value().seconds();
                                const double price = FirestoreDao::get_num_as_double(tx.Get("price"));
                                const double fee = FirestoreDao::get_num_as_double(tx.Get("fee")); 
                                const double shares = FirestoreDao::get_num_as_double(tx.Get("shares")); 
                                const auto& type = tx.Get("type").string_value();
                                const auto& my_broker = tx.Get("broker").string_value();
                                builder->addTx(my_broker, my_symbol, type, price, shares, fee, date);
                            }
                        }
                        else{
                            LOG(WARNING) << "Cannot get tx: symbol = [" << my_symbol << "]\n";
                            builder->rm_stock(my_symbol);
                        }
                    });
                }
            }
            else{
                builder->failed();
                std::ostringstream ss;
                ss << "Failed to query stock tx , error code=" << future.error();
                throw std::runtime_error(ss.str());
            }
        });
    }

    StringsBuilder get_known_stocks()
    {
        const auto& f = get_stocks(nullptr); 
        if(Await(f, "get known stocks")){
            const auto& docs = f.result()->documents();
            StringsBuilder builder(docs.size());
            for(const auto& stock: docs){
                builder.add(stock.id());
            }
            return builder;
        }
        else throw std::runtime_error("Failed to get known stocks");
    }

    void get_non_fund_symbols(std::function<void(Strings*)> onResult)
    {
        _firestore->Collection(COLLECTION_INSTRUMENTS).WhereNotEqualTo("type", FieldValue::String("Funds"))
            .Get().OnCompletion([onResult](const Future<QuerySnapshot>& future){
                const auto& docs = future.result()->documents();
                Strings* symbols = new Strings(docs.size());
                for(const auto& d: docs){
                    symbols->add(d.id());
                }
                onResult(symbols);
            });
    }

    void get_latest_quotes(LatestQuotesBuilder* builder, int num, const char **symbols_head)
    {
        assert(num!=0 && symbols_head != nullptr);
        auto const& q = _firestore->Collection(COLLECTION_INSTRUMENTS);
        filter_where_in(q, symbols_head, num,[builder](const Future<QuerySnapshot>& future){
             FirestoreDao::sub_collection(builder, future, FirestoreDao::COLLECTION_QUOTES, 
                [](const Query& q){ return q.OrderBy("date", Query::Direction::kDescending).Limit(1); },
                [builder](const std::vector<DocumentSnapshot>& docs){
                    const auto& quote = docs.front();
                    const auto& symbol = quote.Get("instrument").string_value();
                    const auto& price = quote.Get("price");
                    double p = 0.0;
                    if(price.is_valid()){
                        p = FirestoreDao::get_num_as_double(price);
                    }
                    else{
                        p = FirestoreDao::get_num_as_double(quote.Get("rate"));
                    }
                    const auto& date = quote.Get("date").timestamp_value().seconds();
                    builder->add_quote(symbol, date, p);
                });
        });
    }

    void add_tx(const char* broker, const char* symbol, double shares, double price, double fee, const char* side, timestamp date,
                OnDone onDone,void* caller_provided_param)
    {
        const auto& stock = _firestore->Collection(COLLECTION_INSTRUMENTS).Document(std::string(symbol));
        const auto& tx = stock.Collection(FirestoreDao::COLLECTION_TX);
        const auto tm = gmtime(&date);
        char yyyymmdd[10];
        strftime(yyyymmdd,sizeof(yyyymmdd)/sizeof(char), "%Y%m%d", tm);    
        tx.Document(std::string(yyyymmdd)).Set(
            {
                {"instrument_id", FieldValue::String(symbol)},
                {"broker", FieldValue::String(broker)},
                {"type", FieldValue::String(side)},
                {"price", FieldValue::Double(price)},
                {"shares", FieldValue::Double(shares)},
                {"fee", FieldValue::Double(fee)},
                {"date", FieldValue::Timestamp(Timestamp::FromTimeT(date))},
            }
        ).OnCompletion([onDone, caller_provided_param](const Future<void>& future) {
            if (future.error() == Error::kErrorOk) {
                onDone(caller_provided_param);
            }   
            else{
                throw std::runtime_error("Failed to add tx");
            } 
        });
    }
private:
    template<typename T>
    static void sub_collection(Builder<T>* builder,
                               const Future<QuerySnapshot>& parent_doc_future, 
                               const char* sub_collection_name,
                               //todo: crashes if it is defined to return reference: std::function<const Query&(const Query&)>
                               //https://stackoverflow.com/questions/32871606/odd-return-behavior-with-stdfunction-created-from-lambda-c
                               std::function<const Query(const Query&)> filter,
                               std::function<void(const std::vector<DocumentSnapshot>&)> callback)
    {
        auto errCode = parent_doc_future.error();
        if( errCode == Error::kErrorOk){
            LOG(DEBUG) << "Got " << parent_doc_future.result()->size() << " parent docs\n";
            for(const auto& sub: parent_doc_future.result()->documents()){
                auto name = sub.Get("name").string_value();
                auto id = sub.id();
                LOG(DEBUG) << "Getting sub collection of parent doc id=" << id << " name=" << name << "\n";
                const auto& q = filter(sub.reference().Collection(sub_collection_name)).Get();
                q.OnCompletion([builder,callback](const Future<QuerySnapshot>& f) {
                    builder->alloc->inc_counter();
                    LOG(DEBUG) << "Got sub collection for #" << builder->alloc->counter << " parent doc\n";
                    if(f.error() == Error::kErrorOk){
                        const auto* doc = f.result();
                        if(!doc->empty()){
                            callback(doc->documents());
                        }
                    }
                    else{
                        builder->failed();
                        throw std::runtime_error("Failed to query sub collection");
                    }
                    if(builder->alloc->has_enough_counter()){
                        builder->succeed();
                    }
                });
            }
        }
        else{
            builder->failed();
            std::ostringstream ss;
            ss << "Failed to query sub collection "<< sub_collection_name << ", error code=" << errCode;
            throw std::runtime_error(ss.str());
        }
    }

    void filter_where_in(const Query& q, const char **ids_head, int total_num, std::function<void(const Future<QuerySnapshot>&)> callback){
        int num = 0;
        int remaining = total_num;
        LOG(DEBUG) << "Expecting " << total_num << " results\n";
        for(auto id = ids_head; remaining > 0;){ 
            num = remaining > FILTER_WHERE_IN_LIMIT ? FILTER_WHERE_IN_LIMIT : remaining;

            std::vector<FieldValue> ids;
            std::transform(id, id + num, std::back_inserter(ids), [](const char* id){
                LOG(DEBUG) << "Adding id " << id << "\n";
                return FieldValue::String(id);
            });
            LOG(DEBUG) << "Querying  for " << ids.size() << " results\n";
            q.WhereIn(FieldPath::DocumentId(), ids).Get().OnCompletion(callback);
            id += num;
            remaining -= num;
        }
    }

    static inline double get_num_as_double(const FieldValue& fv)
    { 
        if(fv.is_null()) return 0.0;
        return fv.type() == FieldValue::Type::kInteger ? (double)fv.integer_value() : fv.double_value(); 
    }

    template<typename T> T* for_each_doc(const char* collection_name, std::function<T*(const std::vector<DocumentSnapshot>&)> on_all_docs)
    {
        const auto& ref = _firestore->Collection(collection_name);
        auto f = ref.Get();
        if(Await(f, collection_name)){
            const auto& all_docs= f.result()->documents();
            if (!all_docs.empty()) {
                return on_all_docs(all_docs);
            }
        }
        return nullptr;
    }
    template<typename T> T* for_each_broker(std::function<T*(const std::vector<DocumentSnapshot>&)> on_all_brokers){
        return for_each_doc(COLLECTION_BROKERS, on_all_brokers);
    }

    const Future<QuerySnapshot> get_stocks(const char* symbol)
    {
        const auto& q1 = _firestore->Collection(COLLECTION_INSTRUMENTS)
            .WhereIn("type", {FieldValue::String("Stock"), FieldValue::String("ETF")});
        const auto& q2 = symbol == nullptr ? q1 : q1.WhereEqualTo("name", FieldValue::String(symbol));
        LOG(DEBUG) << "Querying stocks symbol=" << NULL_STR(symbol) << "\n";
        return q2.Get();
    }
};

const char FirestoreDao::COLLECTION_BROKERS[] = "Brokers";
const char FirestoreDao::COLLECTION_INSTRUMENTS[] = "Instruments";
const char FirestoreDao::COLLECTION_TX[] = "tx";
const char FirestoreDao::COLLECTION_QUOTES[] = "quotes";

IDataStorage * create_cloud_instance(OnDone onInitDone) { return new Storage<FirestoreDao>(onInitDone); }
