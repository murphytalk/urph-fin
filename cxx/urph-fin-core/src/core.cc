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

std::string CashBalance::to_str()
{
    std::stringstream ss;
    ss << balance << " " << ccy;
    return ss.str();
}

template<typename Wrapper, typename T>
void free_multiple_structs(Wrapper* data){
    T* p = data->head();
    for(int i = 0; i < data->num ; ++i, ++p){
        p->~T();
    }
}

Broker::Broker(const std::string&n, int ccy_num, cash_balance* first_ccy_balance)
{
    name = copy_str(n);
    num = ccy_num;
    cash_balances = first_ccy_balance;
}

Broker::~Broker(){
    LOG(DEBUG) << "freeing broker " << name << " : cash balances: [";
    delete[] name;
    free_multiple_structs<Broker, CashBalance>(this);
    delete[] cash_balances;
    LOG(DEBUG) << "] - freed!\n";
}

CashBalance* Broker::head() { return static_cast<CashBalance*>(cash_balances); }

std::string Broker::to_str(){
    std::stringstream ss;
    ss << name << " : [";
    CashBalance* p = head();
    for(int i = 0; i < num ; ++i){
        ss << p->to_str() << ",";
        ++p;
    }
    ss << "]";
    return ss.str();
}

AllBrokers::AllBrokers()
{
    num = 0;
    brokers = nullptr;
}

AllBrokers::AllBrokers(int n, broker* broker)
{
    num = n;
    brokers = broker;
}

AllBrokers::~AllBrokers()
{
    LOG(DEBUG) << "freeing " << num << " brokers \n";
    free_multiple_structs<AllBrokers, Broker>(this);
    delete []brokers;
}

Broker* AllBrokers::head() { return static_cast<Broker*>(brokers); }

std::string AllBrokers::to_str()
{
    std::stringstream ss;
    Broker* p = head();
    for(int i = 0; i < num ; ++i){
        ss << p->to_str() << "\n";
        ++p;
    }
    return ss.str();
}

static_assert(sizeof(AllBrokers) == sizeof(all_brokers));
static_assert(sizeof(Broker) == sizeof(broker));
static_assert(sizeof(CashBalance)  == sizeof(cash_balance));

class Cloud{
private:
    App*  _firebaseApp;
    Auth* _firebaseAuth;
    Firestore* _firestore;
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

    AllBrokers* get_brokers()
    {
        return for_each_broker<AllBrokers>([](const std::vector<DocumentSnapshot>& all_brokers) -> AllBrokers*{
            LOG(DEBUG) << "Loading " << all_brokers.size() << " brokers\n";
            broker* brokers = new broker [all_brokers.size()];
            broker* brokers_head = brokers;
            for (const auto& broker : all_brokers) {
                const auto& cash = broker.Get("cash");
                const auto& all_ccys = cash.map_value();
                LOG(DEBUG) << " " << broker.id() << "\n";
                cash_balance * balances = new cash_balance [all_ccys.size()];
                cash_balance * head = balances;
                for (const auto& b : all_ccys) {
                    new (balances) CashBalance(b.first, Cloud::get_num_as_double(b.second));
                    LOG(DEBUG) << "  " << balances->balance << " " << balances->ccy << "" "\n";
                    ++balances;
                }
                new (brokers) Broker(broker.id(), all_ccys.size(), head);
                ++brokers;
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
private:
    static inline double get_num_as_double(const FieldValue& fv){ return fv.type() == FieldValue::Type::kInteger ? (double)fv.integer_value() : fv.double_value(); }
    inline CollectionReference get_brokers_ref() const { return _firestore->Collection("Brokers"); }
    template<typename T>
    T* for_each_broker(std::function<T*(const std::vector<DocumentSnapshot>&)> on_all_brokers)
    {
        auto ref = get_brokers_ref();
        auto f = ref.Get();
        if(Await(f, "get brokers")){
            const auto& all_brokers = f.result()->documents();
            if (!all_brokers.empty()) {
                return on_all_brokers(all_brokers);
            }
        }
        return nullptr;
    }
};

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

void urph_fin_core_close()
{
    delete cloud;
}
