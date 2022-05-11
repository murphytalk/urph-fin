#ifndef URPH_FIN_CORE_H_
#define URPH_FIN_CORE_H_

#include <cstddef>
#include <cstdint>

// C interface for other languages (flutter plug-in etc.)

extern "C"
{

typedef void (*OnInitDone)();

bool urph_fin_core_init(OnInitDone);
void urph_fin_core_close();

// Funds
struct cash_balance{
    char* ccy;
    double balance;
};

struct strings{
    int capacity;
    char** strs;
    char** last_str;
};
void free_strings(strings*);

struct broker{
    char* name;
    int num;
    cash_balance* first_cash_balance;
    strings* active_fund_ids;
};
struct all_brokers{
    broker* first_broker;
    int num;
};

broker* get_broker(const char* name);
void free_broker(broker*);

all_brokers* get_brokers();
void free_brokers(all_brokers*);

strings* get_all_broker_names(size_t*);

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
    double profit;
    double ROI;
    timestamp date; 
};

struct fund_portfolio{
    int num;
    fund* first_fund;
};

typedef void (*OnFunds)(fund_portfolio*, void* param);
// return all funds if broker == nullptr
void get_funds(int num, const char **fund_ids, OnFunds, void*param);
void get_active_funds(const char* broker, OnFunds, void*param);
void free_funds(fund_portfolio*);

struct fund_sum
{
    double market_value;
    double capital;
    double profit;
    double ROI;
};
fund_sum calc_fund_sum(fund_portfolio* portfolio);

// Stocks & ETF
strings* get_known_stocks(const char* broker);

struct stock
{
    const char* symbol;
    const char* currency;
};

const unsigned char BUY   = 0;
const unsigned char SELL  = 1;
const unsigned char SPLIT = 2;
struct stock_tx
{
    const char* broker;
    double fee;
    double shares;
    double price;
    unsigned char side; 
    timestamp date; 
};

struct stock_tx_list
{
    int num;
    stock_tx* first_tx;
};

struct stock_with_tx
{
    stock* instrument;
    stock_tx_list* tx_list;
};

struct stock_portfolio
{
    int num;
    stock_with_tx* first_stock_with_tx;
};

typedef void (*OnAllStockTx)(stock_portfolio*, void* param);
// broker = null => all brokers
void get_stock_tx_list(const char* broker, const char* symbol, OnAllStockTx, void* caller_provided_param);
void free_stock_tx_list(stock_portfolio*);
struct stock_balance
{
    double shares;
    double fee;
    // negative value => original investment still in market
    double liquidated;
    double vwap;
};
stock_balance get_stock_balance(stock_tx_list* tx);


}
#endif // URPH_FIN_CORE_H_
