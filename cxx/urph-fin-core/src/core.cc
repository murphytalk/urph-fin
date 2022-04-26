#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#include <pwd.h>
#endif  // _WIN32


#include <cstdlib>
#include <cassert>

#ifdef _WIN32
#include <windows.h>
#endif  // _WIN32

#include <memory>
#include <stdexcept>
#include <sstream> 
#include <string>
#include <functional>
#include <vector>

// 3rd party
#include "aixlog.hpp"
#include "yaml-cpp/yaml.h"

#include "urph-fin-core.hxx"
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

static std::string get_home_dir()
{
    const char *homedir;
#ifdef _WIN32
    //HOMEPATH or userprofile
#else
    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }
#endif
    // compiler is smart enough to either do return-value-optimization or use move
    //https://stackoverflow.com/questions/4986673/c11-rvalues-and-move-semantics-confusion-return-statement
    return std::string(homedir);
}

static YAML::Node load_cfg(){
    return YAML::LoadFile(get_home_dir() + "/.finance-credentials/urph-fin.yaml");
}

// dont forget to free
static char* copy_str(const std::string& str)
{
    char* p = new char[str.size() + 1];
    strncpy(p, str.c_str(), str.size());
    p[str.size()] = 0;
    return p;
}

CashBalance::CashBalance(const std::string& n, float v)
{
    ccy = copy_str(n);
    balance = v;        
}

CashBalance::~CashBalance()
{
    LOG(DEBUG) << " ccy= " << ccy << " ";
    delete[] ccy;
}

ActiveFund::ActiveFund(const std::string& i)
{
    id = copy_str(i);
}
ActiveFund::~ActiveFund()
{
    LOG(DEBUG) << " id= " << id << " ";
    delete []id;
}

Broker::Broker(const std::string&n, int ccy_num, cash_balance* first_ccy_balance, int active_funds_num, active_fund* first_fund)
{
    name = copy_str(n);
    num = ccy_num;
    first_cash_balance = first_ccy_balance;

    this->active_funds_num = active_funds_num;
    this->first_active_fund = first_fund;
}

Broker::~Broker(){
    LOG(DEBUG) << "freeing broker " << name << " : cash balances: [";
    delete[] name;

    free_multiple_structs<Broker, CashBalance>(this);
    delete[] first_cash_balance;

    LOG(DEBUG) << "] - active funds [";
    free_multiple_structs<Broker, ActiveFund>(this, active_fund_tag());
    delete[] first_active_fund;
    LOG(DEBUG) << "] - freed!\n";
}

AllBrokers::AllBrokers(int n, broker* broker)
{
    num = n;
    first_broker = broker;
}

AllBrokers::~AllBrokers()
{
    LOG(DEBUG) << "freeing " << num << " brokers \n";
    free_multiple_structs<AllBrokers, Broker>(this);
    delete []first_broker;
}

FundPortfolio::FundPortfolio(int n, fund* f)
{
    num = n;
    first_fund = f;
}
FundPortfolio::~FundPortfolio()
{
    free_multiple_structs<FundPortfolio, Fund>(this);
    delete []first_fund;
}

Fund::Fund(const std::string& b,  const std::string&n,  const std::string&i, int a, double c, double m, double p, timestamp d)
{
    broker = copy_str(b);
    name = copy_str(n);
    id = copy_str(i);
    amount = a;
    capital = c;
    market_value = m;
    price = p;
    date = d;
}    

Fund::~Fund()
{
    delete []broker;
    delete []name;
    delete []id;
}

template<typename T > struct PlacementNew{
    T* head;
    T* current;
    int counter;
    int max_counter;
    PlacementNew(int max_num)
    {
        max_counter = max_num;
        counter = 0;
        head = new T[max_num];
        current = head;
    }
    int allocated_num()
    {
        return current - head;
    }
    bool has_enough_counter()
    {
        return counter >= max_counter;
    }
    inline void inc_counter() { ++counter; }
};

class Cloud{
private:
    App*  _firebaseApp;
    Auth* _firebaseAuth;
    Firestore* _firestore;

    static const char COLLECTION_BROKERS [];
    static const int FILTER_BY_ID_LIMIT = 10;
public:
    Cloud(OnInitDone onInitDone){
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
        auto userCfg = cfg["user"];
        auto email = userCfg["email"].as<std::string>();
        auto passwd = userCfg["password"].as<std::string>();
        LOG(INFO) << "Log in as " << email << "\n"; 
        _firebaseAuth->SignInWithEmailAndPassword(email.c_str(), passwd.c_str())
#endif
        .OnCompletion(c, (void*)onInitDone);
        
    }
    ~Cloud(){
        _firebaseAuth->SignOut();
        delete _firestore;
        delete _firebaseAuth;
        delete _firebaseApp;
        LOG(DEBUG) << "Cloud instance destroyed\n";
    }

    broker* get_broker(const char* name)
    {
        const auto& doc_ref = _firestore->Collection(COLLECTION_BROKERS).Document(name);
        const auto& f = doc_ref.Get();
        if(Await(f, "broker by name")){
            const auto& broker = *f.result();
            return create_broker(broker, [](const std::string&n, int ccy_num, cash_balance* first_ccy_balance, int active_funds_num, active_fund* first_fund){
                return new Broker(n, ccy_num, first_ccy_balance, active_funds_num, first_fund);
            });
        }
        else{
            return nullptr;
        }
    }

    void free_broker(broker* broker)
    {
        delete static_cast<Broker*>(broker);
    }

    AllBrokers* get_brokers()
    {
        return for_each_broker<AllBrokers>([](const std::vector<DocumentSnapshot>& all_brokers) -> AllBrokers*{
            LOG(DEBUG) << "Loading " << all_brokers.size() << " brokers\n";
            broker* brokers = new broker [all_brokers.size()];
            broker* brokers_head = brokers;
            for (const auto& broker : all_brokers) {
                Cloud::create_broker(broker, [&brokers](const std::string&n, int ccy_num, cash_balance* first_ccy_balance, int active_funds_num, active_fund* first_fund){
                    return new (brokers++) Broker(n, ccy_num, first_ccy_balance, active_funds_num, first_fund);
                });
            }
            return new AllBrokers(all_brokers.size(), brokers_head);
        });
    }

    void free_brokers(all_brokers* b){
        AllBrokers* brokers = static_cast<AllBrokers*>(b);
        delete brokers;
    }

    char** get_all_broker_names(size_t& size)
    {
        size = 0;
        return for_each_broker<char*>([&size](const std::vector<DocumentSnapshot>& all_brokers) -> char**{
            char **all_broker_names = new char* [all_brokers.size()];
            for (size_t i = 0; i < all_brokers.size(); ++i){
                auto broker_name = all_brokers[i].id();
                all_broker_names[i] = copy_str(broker_name);
            }
            size = all_brokers.size();
            return all_broker_names;
        });
    }
    void free_all_broker_names(char** names, size_t size)
    {
        for(size_t i = 0; i < size; ++i){
            delete[] names[i];
        }
        delete []names;
    }

    void get_funds(int funds_num, const char **fund_ids_head, OnFunds onFunds, void* onFundsCallerProvidedParam)
    {
        // cannot use auto var allocated in stack, as it will be freed once out of the context of OnCompletion()
        // and becomes invalid when the lambda is called
        auto* fund_alloc = new PlacementNew<fund>(funds_num);

        int num = 0;
        int remaining = funds_num;
        for(auto fund_ids = fund_ids_head; remaining > 0;){ 
            num = remaining > FILTER_BY_ID_LIMIT ? FILTER_BY_ID_LIMIT : remaining;

            std::vector<FieldValue> ids;
            std::transform(fund_ids, fund_ids + num, std::back_inserter(ids), [](const char* id){
                LOG(DEBUG) << "Adding fund id " << id << "\n";
                return FieldValue::String(id);
            });

            const auto& q = _firestore->Collection("Instruments").WhereIn("id", ids);
            q.Get().OnCompletion([onFunds, onFundsCallerProvidedParam, fund_alloc](const Future<QuerySnapshot>& future) {
                if (future.error() == Error::kErrorOk) {
                    for(const auto& the_fund: future.result()->documents()){
                        // find latest tx
                        auto fund_name = the_fund.Get("name").string_value();
                        auto fund_id = the_fund.id();
                        LOG(DEBUG) << "getting tx of fund id=" << fund_id << " name=" << fund_name << "\n";
                        the_fund.reference().Collection("tx")
                            .OrderBy("date", Query::Direction::kDescending).Limit(1)
                            .Get().OnCompletion([onFunds,onFundsCallerProvidedParam, fund_alloc](const Future<QuerySnapshot>& future) {
                                fund_alloc->inc_counter();
                                LOG(DEBUG) << "tx result for #" << fund_alloc->counter << " fund\n";
                                if(future.error() == Error::kErrorOk){
                                    if(!future.result()->empty()){
                                        const auto& docs = future.result()->documents();
                                        const auto& tx_ref = docs.front();
                                        const auto& broker = tx_ref.Get("broker").string_value();
                                        const auto& name = tx_ref.Get("instrument_name").string_value();
                                        const auto& id   = tx_ref.Get("instrument_id").string_value();
                                        const auto amt = tx_ref.Get("amount").integer_value();
                                        double capital = Cloud::get_num_as_double(tx_ref.Get("capital"));
                                        double market_value = Cloud::get_num_as_double(tx_ref.Get("market_value"));
                                        double price = Cloud::get_num_as_double(tx_ref.Get("price"));
                                        timestamp date = tx_ref.Get("date").timestamp_value().seconds();
                                        LOG(DEBUG) << "got tx of broker " << broker << ".fund id="<<id<<",name="<<name << "\n";
                                        new (fund_alloc->current++) Fund(
                                            broker, name, id,
                                            (int)amt,
                                            capital,
                                            market_value,
                                            price,
                                            date
                                        );
                                        LOG(DEBUG) << "No." << fund_alloc->allocated_num() << " created: tx of broker " << broker << ".fund id="<<id<<",name="<<name << "\n";
                                    }
                                }
                                else{
                                    LOG(ERROR) << "Failed to query tx from funds \n";
                                }
                                if(fund_alloc->has_enough_counter()){
                                    onFunds(new FundPortfolio(fund_alloc->allocated_num(), fund_alloc->head), onFundsCallerProvidedParam);
                                    delete fund_alloc;
                                }
                            });
                    }
                }
                else{
                    LOG(ERROR) << "Failed to query funds \n";
                    onFunds(nullptr, onFundsCallerProvidedParam);
                    delete fund_alloc;
                }
            });

            fund_ids += num;
            remaining -= num;
        }
    }

    void free_funds(fund_portfolio *f)
    {
        auto p = static_cast<FundPortfolio*>(f);
        delete p;
    }

private:
    static inline double get_num_as_double(const FieldValue& fv)
    { 
        if(fv.is_null()) return 0.0;
        return fv.type() == FieldValue::Type::kInteger ? (double)fv.integer_value() : fv.double_value(); 
    }

    static Broker* create_broker(
        const DocumentSnapshot& broker, 
        std::function<Broker*(const std::string&/*name*/, int/*ccy_num*/, cash_balance* /*first_ccy_balance*/, int/*active_funds_num*/, active_fund* /*first_fund*/)> create_func)
    {
        const auto& cash = broker.Get("cash");
        const auto& all_ccys = cash.map_value();
        LOG(DEBUG) << " " << broker.id() << "\n";
        cash_balance* balances = new cash_balance [all_ccys.size()];
        cash_balance* head = balances;
        for (const auto& b : all_ccys) {
            LOG(DEBUG) << "  " << b.first << " " << b.second << "" "\n";
            new (balances++) CashBalance(b.first, Cloud::get_num_as_double(b.second));
        }

        const auto& active_funds = broker.Get("active_funds").array_value();
        active_fund* funds = new active_fund[active_funds.size()];
        active_fund* fund_head = funds;
        for(const auto& f: active_funds){
            new (funds++) ActiveFund(f.string_value());
        }
                
        return create_func(broker.id(), all_ccys.size(), head, active_funds.size(), fund_head);
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
};

const char Cloud::COLLECTION_BROKERS[] = "Brokers";

Cloud *cloud;

bool urph_fin_core_init(OnInitDone onInitDone)
{
    std::vector<AixLog::log_sink_ptr> sinks;
    
    auto log_file = getenv("LOGFILE");
    auto verbose = getenv("VERBOSE");
    auto log_lvl = verbose == nullptr ? AixLog::Severity::info : AixLog::Severity::debug;
    if(log_file == nullptr)
        sinks.push_back(std::make_shared<AixLog::SinkCout>(log_lvl));
    else  
        sinks.push_back(std::make_shared<AixLog::SinkFile>(log_lvl, log_file)); 
    AixLog::Log::init(sinks);
    
    LOG(DEBUG) << "urph-fin-core initializing\n";

    try{
        cloud = new Cloud(onInitDone);
        return true;
    }
    catch(const std::exception& e){
        LOG(ERROR) << "Failed to init cloud service: " << e.what() << "\n";
        return false;
    }
}

// https://stackoverflow.com/questions/60879616/dart-flutter-getting-data-array-from-c-c-using-ffi
all_brokers* get_brokers()
{
    assert(cloud != nullptr);
    return cloud->get_brokers();
}

void free_brokers(all_brokers* data)
{
    assert(cloud != nullptr);
    cloud->free_brokers(data);
}


broker* get_broker(const char* name)
{
    assert(cloud != nullptr);
    return cloud->get_broker(name);
}

void free_broker(broker* b)
{
    assert(cloud != nullptr);
    cloud->free_broker(b);
}

char** get_all_broker_names(size_t* size)
{
    assert(cloud != nullptr);
    return cloud->get_all_broker_names(*size);
}

void free_broker_names(char** n,size_t size)
{
    assert(cloud != nullptr);
    cloud->free_all_broker_names(n, size);
}

void get_funds(int num, const char **fund_ids, OnFunds onFunds, void*param)
{
    assert(cloud != nullptr);
    cloud->get_funds(num, fund_ids, onFunds, param);
}

void get_active_funds(const char* broker_name, OnFunds onFunds, void*param)
{
    Iterator<Broker> *begin, *end;
    Broker* broker =  nullptr;
    AllBrokers* all_brokers = nullptr;

    int fund_num = 0;
    std::vector<Broker*> all_broker_pointers;

    if(broker_name != nullptr){
        broker = static_cast<Broker*>(get_broker(broker_name));
        if(broker == nullptr){
            onFunds(nullptr, param);
            return;
        }
        fund_num = broker->size(Broker::active_fund_tag());
        all_broker_pointers.push_back(broker);
    }
    else{
        all_brokers = static_cast<AllBrokers*>(get_brokers());
        for(Broker& b: *all_brokers){
            fund_num += b.size(Broker::active_fund_tag());
            all_broker_pointers.push_back(&b); 
        }
    }

    const char ** ids = new const char* [fund_num];
    const char ** ids_head = ids;
    for(auto* broker: all_broker_pointers){
        for(auto it = broker->fund_begin(); it!= broker->fund_end(); ++it){
            auto& f = *it;
            *ids++ = f.id;
        }         
    }
    get_funds(fund_num, ids_head, onFunds, param);       

    delete []ids_head;

    delete broker;
    delete all_brokers;
}

void free_funds(fund_portfolio* f)
{
    assert(cloud != nullptr);
    cloud->free_funds(f);
}
void urph_fin_core_close()
{
    delete cloud;
}
