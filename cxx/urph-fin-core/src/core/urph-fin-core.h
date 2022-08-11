#ifndef URPH_FIN_CORE_H_
#define URPH_FIN_CORE_H_

#include <cstddef>
#include <cstdint>

// C interface for other languages (flutter plug-in etc.)

extern "C"
{

typedef void (*OnDone)(void*param);

bool urph_fin_core_init(OnDone);
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
strings* get_known_stocks();

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
    //owner of the stock pointer is stock_portfolio, leave the memory free to it
    stock* instrument;
    stock_tx_list* tx_list;
};

struct stock_portfolio
{
    int num;
    stock* first_stock;
    stock_with_tx* first_stock_with_tx;
};

typedef void (*OnAllStockTx)(stock_portfolio*, void* param);
// broker = null => all brokers
void get_stock_portfolio(const char* broker, const char* symbol, OnAllStockTx callback, void* caller_provided_param);
void free_stock_portfolio(stock_portfolio *p);
struct stock_balance
{
    double shares;
    double fee;
    // negative value => original investment still in market
    double liquidated;
    double vwap;
};
stock_balance get_stock_balance(stock_tx_list* tx);

struct quote
{
    char*     symbol;
    timestamp date;
    double    rate;
};

struct quotes
{
    int num;
    quote* first;
};
typedef void (*OnQuotes)(quotes*, void* param);
void get_quotes_async(int num, const char **symbols_head, OnQuotes onQuotes, void* caller_provided_param);
quotes* get_quotes(int num, const char **symbols_head);
void free_quotes(quotes* q);


void add_stock_tx(const char* broker, const char* symbol, double shares, double price, double fee,const char* side, timestamp date, OnDone, void*);

}
#endif // URPH_FIN_CORE_H_
