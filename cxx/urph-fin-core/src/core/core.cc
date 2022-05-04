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
