#ifndef URPH_FIN_CORE_H_
#define URPH_FIN_CORE_H_

// C interface for other languages (flutter plug-in etc.)

extern "C"
{

typedef void (*OnInitDone)();

bool urph_fin_core_init(OnInitDone);
void urph_fin_core_close();

struct cash_balance{
    char* ccy;
    double balance;
};

struct active_fund{
    char* id;
};

struct broker{
    char* name;
    int num;
    cash_balance* first_cash_balance;
    int active_funds_num;
    active_fund* first_active_fund;
};
struct all_brokers{
    broker* first_broker;
    int num;
};

all_brokers* get_brokers();
void free_brokers(all_brokers*);

char** get_all_broker_names(size_t*);
void free_broker_names(char**,size_t);

// seconds since epoch
typedef int64_t timestamp;

struct fund{
    const char* broker;
    const char* name;
    const char* id;
    int amount;
    double capital;
    double market_value;
    double price;
    timestamp date; 
};

struct fund_portfolio{
    int num;
    fund* first_fund;
};

typedef void (*OnFunds)(fund_portfolio*);
// return all funds if broker == nullptr
void get_funds(const char* broker, OnFunds);
void free_funds(fund_portfolio*);

}
#endif // URPH_FIN_CORE_H_
