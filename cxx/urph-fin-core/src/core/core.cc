#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#include <pwd.h>
#endif  // _WIN32

#include <cstdlib>
#include <cstring>
#include <cassert>
#include <numeric>

#include "storage/storage.hxx"
#include "urph-fin-core.hxx"

// 3rd party
#include "aixlog.hpp"
#include "yaml-cpp/yaml.h"

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

Config load_cfg(){
    auto cfg = YAML::LoadFile(get_home_dir() + "/.finance-credentials/urph-fin.yaml");
    auto userCfg = cfg["user"];
    return Config{userCfg["email"].as<std::string>(), userCfg["password"].as<std::string>()};
}

// dont forget to free
char* copy_str(const std::string& str)
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

Strings::Strings(int n)
{
    capacity = n;
    strs = new char* [n];    
    last_str = strs;
}

void Strings::add(const std::string& i)
{
    if(size() == capacity){
        std::stringstream ss;
        ss << "Could not add string [" << i << "] : exceeding capacity " << capacity;
        throw std::runtime_error(ss.str());
    }
    *last_str++ = copy_str(i);
}

int Strings::size()
{
    return last_str - strs;
}

Strings::~Strings()
{
    for(char** p=strs; p!=last_str; ++p){
        LOG(DEBUG) << *p << ",";
        delete []*p;
    }
    delete []strs;
}

void free_strings(strings* ss)
{
    delete static_cast<Strings*>(ss);
}

Broker::Broker(const std::string&n, int ccy_num, cash_balance* first_ccy_balance, strings* active_funds)
{
    name = copy_str(n);
    num = ccy_num;
    first_cash_balance = first_ccy_balance;
    active_fund_ids = active_funds;
}

Broker::~Broker(){
    LOG(DEBUG) << "freeing broker " << name << " : cash balances: [";
    delete[] name;

    free_placement_allocated_structs<Broker, CashBalance>(this);
    delete[] first_cash_balance;

    LOG(DEBUG) << "] - active funds [";
    free_strings(active_fund_ids);
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
    free_placement_allocated_structs<AllBrokers, Broker>(this);
    delete []first_broker;
}

FundPortfolio::FundPortfolio(int n, fund* f)
{
    num = n;
    first_fund = f;
}
FundPortfolio::~FundPortfolio()
{
    free_placement_allocated_structs<FundPortfolio, Fund>(this);
    delete []first_fund;
}

Fund::Fund(const std::string& b,  const std::string&n,  const std::string&i, int a, double c, double m, double prc, double p, double r, timestamp d)
{
    broker = copy_str(b);
    name = copy_str(n);
    id = copy_str(i);
    amount = a;
    capital = c;
    market_value = m;
    price = prc;
    profit = p;
    ROI = r;
    date = d;
}    

Fund::~Fund()
{
    delete []broker;
    delete []name;
    delete []id;
}

Quote::Quote(const std::string& s, timestamp t, double r)
{
    symbol = copy_str(s);
    date = t;
    rate = r;
}

Quote::~Quote()
{
    delete []symbol;
}

IStorage *storage;

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
        storage = create_firestore_instance(onInitDone);
        return true;
    }
    catch(const std::exception& e){
        LOG(ERROR) << "Failed to init storage service: " << e.what() << "\n";
        return false;
    }
}

void urph_fin_core_close()
{
    delete storage;
}

#define TRY try{
#define CATCH(error_ret) }catch(const std::runtime_error& e) { LOG(ERROR) << e.what(); return error_ret; }    
#define CATCH_NO_RET }catch(const std::runtime_error& e) { LOG(ERROR) << e.what(); }    

// https://stackoverflow.com/questions/60879616/dart-flutter-getting-data-array-from-c-c-using-ffi
all_brokers* get_brokers()
{
    assert(storage != nullptr);
    return storage->get_brokers();
}

void free_brokers(all_brokers* b)
{
    AllBrokers* brokers = static_cast<AllBrokers*>(b);
    delete brokers;
}

broker* get_broker(const char* name)
{
    assert(storage != nullptr);
    TRY
    return storage->get_broker(name);
    CATCH(nullptr)
}

void free_broker(broker* b)
{
    delete static_cast<Broker*>(b);
}

strings* get_all_broker_names(size_t* size)
{
    assert(storage != nullptr);
    TRY
    return storage->get_all_broker_names(*size);
    CATCH(nullptr)
}


void get_funds(int num, const char **fund_ids, OnFunds onFunds, void*param)
{
    assert(storage != nullptr);
    TRY
    storage->get_funds(num, fund_ids, onFunds, param);
    CATCH_NO_RET
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
            *ids++ = *it;
        }
    }
    TRY
    get_funds(fund_num, ids_head, onFunds, param);       
    CATCH_NO_RET

    delete []ids_head;

    delete broker;
    delete all_brokers;
}

void free_funds(fund_portfolio* f)
{
    auto p = static_cast<FundPortfolio*>(f);
    delete p;
}


fund_sum calc_fund_sum(fund_portfolio* portfolio)
{
    struct fund_sum init = { 0.0, 0.0, 0.0 };

    auto r = std::accumulate(portfolio->first_fund, portfolio->first_fund + portfolio->num, init, [](fund_sum& sum, fund& f){ 
        sum.capital += f.capital; 
        sum.market_value += f.market_value; 
        sum.profit +=  f.market_value - f.capital;
        sum.ROI +=  sum.profit/sum.capital;
        return sum;
    });

    r.ROI /= portfolio->num;

    return r;
}

strings* get_known_stocks(const char* broker)
{
    assert(storage != nullptr);
    TRY
    return storage->get_known_stocks(broker);
    CATCH(nullptr)
}

void get_stock_portfolio(const char* broker, const char* symbol, OnAllStockTx callback, void* caller_provided_param)
{
    assert(storage != nullptr);
    TRY
    storage->get_stock_portfolio(broker, symbol,callback, caller_provided_param);  
    CATCH_NO_RET
}

void free_stock_portfolio(stock_portfolio *p)
{
    auto port = static_cast<StockPortfolio*>(p);
    delete port;
}

stock_balance get_stock_balance(stock_tx_list* tx)
{
    auto p = static_cast<StockTxList*>(tx);
    return p->calc();
}