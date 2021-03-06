#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#include <pwd.h>
#endif  // _WIN32



#ifdef _WIN32
#include <windows.h>
#endif  // _WIN32

#include <memory>
#include <stdexcept>
#include <sstream> 
#include <string>
#include <functional>
#include <vector>

#include "../utils.hxx"

#include "../core/urph-fin-core.hxx"
#include "storage.hxx"

#include "firebase/auth.h"
#include "firebase/auth.h"
#include "firebase/auth/credential.h"
#include "firebase/util.h"
#include "firebase/firestore.h"

using ::firebase::App;
using ::firebase::AppOptions;
using ::firebase::Future;
using ::firebase::FutureBase;
using ::firebase::Variant;
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
  while (future.status() == firebase::kFutureStatusPending && remaining_timeout > 0) {
    remaining_timeout -= kSleepMs;
    ProcessEvents(kSleepMs);
  }

  if (future.status() != firebase::kFutureStatusComplete) {
    LOG(ERROR) << name << " returned an invalid result.\n";
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
    static const int FILTER_WHERE_IN_LIMIT = 10; // Firestore API constrain
public:
    using BrokerType = DocumentSnapshot;
    FirestoreDao(OnInitDone onInitDone){
        _firebaseApp = 
#if defined(__ANDROID__)
            App::Create(GetJniEnv(), GetActivity())
#else
            ::firebase::App::Create()
#endif
        ;

        _firestore = nullptr;
        char buf[200];
        getcwd(buf, 200);
        LOG(DEBUG) << "Started at " << buf << "\n";
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
                (*(OnInitDone)onInitDone)();
            }
            else{
                std::ostringstream ss;
                ss << "Failed to auth : err code - " << err_code << ", err msg - " <<  u.error_message();
                throw std::runtime_error(ss.str());
            }

        };
#if defined(__ANDROID__)
#else
        auto cfg = load_cfg();
        LOG(INFO) << "Log in as " << cfg.email << "\n"; 
        _firebaseAuth->SignInWithEmailAndPassword(cfg.email.c_str(), cfg.password.c_str())
#endif
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

    StringsBuilder* get_all_broker_names(size_t& size)
    {
        size = 0;
        return for_each_broker<StringsBuilder>([&](const std::vector<DocumentSnapshot>& all_brokers) -> StringsBuilder*{
            StringsBuilder* p = new StringsBuilder(all_brokers.size());
            for (size_t i = 0; i < all_brokers.size(); ++i){
                auto broker_name = all_brokers[i].id();
                p->add(broker_name);
            }
            size = all_brokers.size();
            return p;
        });
    }

    void get_funds(FundsBuilder* builder, int funds_num, const char **fund_ids_head)
    {
        // cannot use auto var allocated in stack, as it will be freed once out of the context of OnCompletion()
        // and becomes invalid when the lambda is called
        int num = 0;
        int remaining = funds_num;
        for(auto fund_ids = fund_ids_head; remaining > 0;){ 
            num = remaining > FILTER_WHERE_IN_LIMIT ? FILTER_WHERE_IN_LIMIT : remaining;

            std::vector<FieldValue> ids;
            std::transform(fund_ids, fund_ids + num, std::back_inserter(ids), [](const char* id){
                LOG(DEBUG) << "Adding fund id " << id << "\n";
                return FieldValue::String(id);
            });

            const auto& q = _firestore->Collection(COLLECTION_INSTRUMENTS).WhereIn(FieldPath::DocumentId(), ids);
            q.Get().OnCompletion([builder](const Future<QuerySnapshot>& future) {
                if (future.error() == Error::kErrorOk) {
                    LOG(DEBUG) << "Got " << future.result()->size() << " funds\n";
                    for(const auto& the_fund: future.result()->documents()){
                        // find latest tx
                        auto fund_name = the_fund.Get("name").string_value();
                        auto fund_id = the_fund.id();
                        LOG(DEBUG) << "getting tx of fund id=" << fund_id << " name=" << fund_name << "\n";
                        the_fund.reference().Collection(FirestoreDao::COLLECTION_TX)
                            .OrderBy("date", Query::Direction::kDescending).Limit(1)
                            .Get().OnCompletion([builder](const Future<QuerySnapshot>& future) {
                                builder->fund_alloc->inc_counter();
                                LOG(DEBUG) << "tx result for #" << builder->fund_alloc->counter << " fund\n";
                                if(future.error() == Error::kErrorOk){
                                    if(!future.result()->empty()){
                                        const auto& docs = future.result()->documents();
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
                                        LOG(DEBUG) << "No." << builder->fund_alloc->allocated_num() << " created: tx of broker " << broker << ".fund id="<<id<<",name="<<name << "\n";
                                    }
                                }
                                else{
                                    throw std::runtime_error("Failed to query tx from funds");
                                }
                                if(builder->fund_alloc->has_enough_counter()){
                                    builder->succeed();
                                }
                            });
                    }
                }
                else{
                    builder->failed();
                    throw std::runtime_error("Failed to query funds");
                }
            });

            fund_ids += num;
            remaining -= num;
        }
    }

    void get_stock_portfolio(StockPortfolioBuilder* builder, const char* broker, const char* symbol)
    {
        get_stocks(broker, symbol).OnCompletion([builder](const Future<QuerySnapshot>& future){
            if(future.error() == Error::kErrorOk){
                const auto& stocks = future.result()->documents();
                builder->prepare_stock_alloc(stocks.size());
                for(const auto& stock: stocks){
                    const auto& symbol = stock.id();
                    const auto& ccy = stock.Get("ccy").string_value();
                    builder->add_stock(symbol, ccy);
                    const auto& qs = stock.reference().Collection(FirestoreDao::COLLECTION_TX).Get();
                    qs.OnCompletion([builder, &symbol](const Future<QuerySnapshot>& future){
                        if(future.error() == Error::kErrorOk){
                            const auto& txx = future.result()->documents();
                            builder->prepare_tx_alloc(symbol, txx.size());
                            for(const auto& tx: txx){
                                builder->incr_counter(symbol);
                                const timestamp date = tx.Get("date").timestamp_value().seconds();
                                const double price = FirestoreDao::get_num_as_double(tx.Get("price"));
                                const double fee = FirestoreDao::get_num_as_double(tx.Get("fee")); 
                                const double shares = FirestoreDao::get_num_as_double(tx.Get("shares")); 
                                const auto& type = tx.Get("type").string_value();
                                const auto& broker = tx.Get("broker").string_value();
                                builder->addTx(broker, symbol, type, price, shares, fee, date);
                            }
                        }
                        else{
                            LOG(WARNING) << "Cannot get tx: symbol = [" << symbol << "]\n";
                            builder->rm_stock(symbol);
                        }
                    });
                }
            }
            else{
                builder->failed();
                throw std::runtime_error(" Failed to query stock tx");
            }
        });
    }

    StringsBuilder get_known_stocks(const char* broker)
    {
        const auto& f = get_stocks(broker, nullptr); 
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
private:
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
    const Future<QuerySnapshot> get_stocks(const char* broker, const char* symbol)
    {
        const auto& q1 = _firestore->Collection(COLLECTION_INSTRUMENTS)
            .WhereIn("type", {FieldValue::String("Stock"), FieldValue::String("ETF")});
        const auto& q2 = symbol == nullptr ? q1 : q1.WhereEqualTo("name", FieldValue::String(symbol));
        const auto& q3 = broker == nullptr ? q2 : q2.WhereEqualTo("broker", FieldValue::String(broker));
        LOG(DEBUG) << "Querying stocks: broker=" << NULL_STR(broker) << ",symbol=" << NULL_STR(symbol) << "\n";
        return q3.Get();
    }
};

const char FirestoreDao::COLLECTION_BROKERS[] = "Brokers";
const char FirestoreDao::COLLECTION_INSTRUMENTS[] = "Instruments";
const char FirestoreDao::COLLECTION_TX[] = "tx";

IStorage * create_firestore_instance(OnInitDone onInitDone) { return new Storage<FirestoreDao>(onInitDone); }
