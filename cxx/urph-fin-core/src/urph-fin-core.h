#ifndef URPH_FIN_CORE_H_
#define URPH_FIN_CORE_H_

extern "C"
{

typedef void (*OnInitDone)();

bool urph_fin_core_init(OnInitDone);
void urph_fin_core_close();

struct cash_balance{
    char* ccy;
    double balance;
};
struct broker{
    char* name;
    int num;
    cash_balance* cash_balances;
};
struct all_brokers{
    broker* brokers;
    int num;
};

all_brokers* get_brokers();
void free_brokers(all_brokers*);

char** get_all_broker_names(size_t*);
void free_broker_names(char**,size_t);

// seconds since epoch
typedef int64_t timestamp;

struct fund_balance{
    char* name;
    char* id;
    int amount;
    double capital;
    double market_value;
    double price;
    timestamp update_time; 
};

struct fund_portfolio{
    int num;
    fund_balance* balances;
};

fund_portfolio* get_fund_portfolio(char* broker);
void free_fund_portfolio(fund_portfolio*);

}
#endif // URPH_FIN_CORE_H_
