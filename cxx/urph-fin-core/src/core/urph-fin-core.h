#ifndef URPH_FIN_CORE_H_
#define URPH_FIN_CORE_H_

#include <cstddef>
#include <cstdint>

// C interface for other languages (flutter plug-in etc.)

extern "C"
{

typedef void (*OnDone)(void*);

bool urph_fin_core_init(OnDone, void* caller_provided_param);
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
typedef void (*OnStrings)(strings*, void*);

struct broker{
    char* name;
    int num;
    cash_balance* first_cash_balance;
    char* funds_update_date;
    strings* active_fund_ids;
};
struct all_brokers{
    broker* first_broker;
    int num;
};

typedef void (*OnBroker)(broker*, void* param);
void get_broker(const char* name, OnBroker onBroker, void* param);
void free_broker(broker*);

typedef void (*OnAllBrokers)(all_brokers*, void* param);
void get_brokers(OnAllBrokers onAllBrokers, void* param);
void free_brokers(all_brokers*);


// seconds since epoch
typedef int64_t timestamp;

struct fund{
    const char* broker;
    const char* name;
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
void get_funds(int num, char* fund_update_date, const char **fund_ids, OnFunds, void*param);
void get_active_funds(const char* broker, OnFunds, void*param);
void get_active_funds_from_all_brokers(all_brokers *bks, bool free_the_brokers,OnFunds onFunds, void*param);
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
void get_known_stocks(OnStrings onStrings, void *ctx);

struct stock
{
    const char* symbol;
    const char* currency;
};

const unsigned char BUY   = 0;
const unsigned char SELL  = 1;
const unsigned char SPLIT = 2;
typedef unsigned char SIDE;
struct stock_tx
{
    const char* broker;
    double fee;
    double shares;
    double price;
    SIDE side; 
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
void get_quotes(strings* symbols, OnQuotes onQuotes, void* caller_provided_param);
void free_quotes(quotes* q);


void add_stock_tx(const char* broker, const char* symbol, double shares, double price, double fee,const char* side, timestamp date, OnDone, void*);

/*
  - item_name = Asset  
    - overview_item_container_container
      name = Stock
      item_name = Broker
      - overview_item_container
        name = StockBroker1
        item_name = Currency 
        -overview_item
         name  = JPY
         value = 1,000
         value_in_main_ccy = 1,000
        -overview_item
         name  = USD
         value = 1,000
         value_in_main_ccy = 100,000
      - overview_item_container
        name = StockBroker2
        item_name = Currency 
        -overview_item
         name  = JPY
         value = 1,000
         value_in_main_ccy = 1,000
        -overview_item
         name  = USD
         value = 1,000
         value_in_main_ccy = 100,000
    - overview_item_container_container
      name = Fund
      item_name = Broker
      - overview_item_container
        name = FundBroker1
        item_name = Currency 
        -overview_item
         name  = JPY
         value = 1,000
         value_in_main_ccy = 1,000
        -overview_item
         name  = USD
         value = 1,000
         value_in_main_ccy = 100,000
      - overview_item_container
        name = FundBroker2
        item_name = Currency 
        -overview_item
         name  = JPY
         value = 1,000
         value_in_main_ccy = 1,000
        -overview_item
         name  = USD
         value = 1,000
         value_in_main_ccy = 100,000
*/
struct overview_item{
    char* name;
    char* currency;
    double value;
    double value_in_main_ccy;
    double profit;
    double profit_in_main_ccy;
};

struct overview_item_list{
    int num;
    overview_item* first;
};

struct overview_item_container{
    char* name;
    char* item_name;
    double value_sum_in_main_ccy;
    double profit_sum_in_main_ccy;
    int num;
    overview_item *items;
};

struct overview_item_container_container{
    char* name;
    char* item_name;
    double value_sum_in_main_ccy;
    double profit_sum_in_main_ccy;
    int num;
    overview_item_container* containers;
};

struct overview{
    char* item_name;
    double value_sum_in_main_ccy;
    double profit_sum_in_main_ccy;
    int num;
    overview_item_container_container *first;
};

typedef int AssetHandle;
typedef void (*OnAssetLoaded)(void*param, AssetHandle h);
void load_assets(OnAssetLoaded onLoaded, void* ctx);

strings* get_all_ccy(AssetHandle handle);

// pass quote of the specified symbol to caller, the caller owns the quote pointer
void get_latest_quote_caller_ownership(const char* symbol, OnQuotes onQuotes, void* caller_provided_param);

// this function returns pointer owned and managed by AllAssets
const quote*  get_latest_quote (AssetHandle handle, const char* symbol);

// the quotes* pointer is allocated by this function and the caller is responsible for releasing
const quotes* get_all_ccy_pairs_quote(AssetHandle handle);
const quotes* get_latest_quotes(AssetHandle handle, int num, const char** symbols);
const quotes* get_latest_quotes_delimeter(AssetHandle handle, int num, const char* delimiter, const char* symbols);

void free_assets(AssetHandle handle);

const unsigned char GROUP_BY_ASSET  = 0;
const unsigned char GROUP_BY_BROKER = 1;
const unsigned char GROUP_BY_CCY    = 2;
typedef unsigned char GROUP;
overview* get_overview(AssetHandle asset, const char* main_ccy, GROUP level1_group, GROUP level2_group, GROUP level3_group);
void free_overview(overview*);

overview_item_list* get_sum_group_by_asset(AssetHandle asset_handle, const char* main_ccy);
overview_item_list* get_sum_group_by_broker(AssetHandle asset_handle, const char* main_ccy);
void free_overview_item_list(overview_item_list *list);

}
#endif // URPH_FIN_CORE_H_
