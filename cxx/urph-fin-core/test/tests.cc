#include <gtest/gtest.h>

#include <algorithm>
#include "core/stock.hxx"
#include "storage/storage.hxx"
#include "core/core_internal.hxx"

TEST(TestStrings, Basic)
{
    const char *expected[] = {"123", "456", "789"};

    Strings ss(sizeof(expected) / sizeof(char *));
    ss.add(expected[0]);
    ss.add(expected[1]);
    ss.add(expected[2]);

    ASSERT_EQ(3, ss.size());
    int i = 0;
    char **pp = ss.to_str_array();
    char **head = pp;
    for (char *p : ss)
    {
        ASSERT_STREQ(expected[i], p);
        ASSERT_STREQ(expected[i++], *pp++);
    }
    delete[] head;
}

TEST(TestStock, Basic)
{
    Stock s1(std::string("SYM"), std::string("USD"));
    stock *s2 = &s1;
    // Expect equality.
    ASSERT_STREQ(s2->symbol, "SYM");
    ASSERT_STREQ(s2->currency, "USD");
}

TEST(TestStock, MoveAssignment)
{
    Stock s1(std::string("SYM"), std::string("USD"));
    Stock s2(std::string("NOS"), std::string("JPY"));
    Stock *p = &s2;
    *p = std::move(s1);
    ASSERT_STREQ(s2.symbol, "SYM");
    ASSERT_STREQ(s2.currency, "USD");
    ASSERT_EQ(s1.symbol, nullptr);
    ASSERT_EQ(s1.currency, nullptr);
}

struct stock_test_data
{
    std::string symbol;
    std::string currency;
};

struct stock_tx_test_data
{
    double shares;
    double price;
    double fee;
    std::string side;
    std::string broker;
    timestamp date;
};

class MockedStocksDao
{
    std::vector<stock_tx_test_data> **all_tx;
    std::vector<stock_test_data> &stocks;

public:
    MockedStocksDao(std::vector<stock_test_data> &s, std::vector<stock_tx_test_data> **tx) : stocks(s), all_tx(tx) {}
    typedef int BrokerType;
    BrokerType get_broker_by_name(const char *) { return 0; }
    std::string get_broker_name(const BrokerType &) { return std::string(""); }
    BrokerBuilder get_broker_cash_balance_and_active_funds(const BrokerType &)
    {
        return BrokerBuilder(0, 0);
    }
    AllBrokerBuilder<MockedStocksDao, BrokerType> *get_brokers() { return nullptr; }
    StringsBuilder *get_all_broker_names() { return nullptr; }
    void get_funds(FundsBuilder *, int, const char **) {}
    StringsBuilder get_known_stocks() { return StringsBuilder(0); }
    void get_non_fund_symbols(std::function<void(Strings *)> onResult) {}
    void get_latest_quotes(LatestQuotesBuilder *builder, int num, const char **symbols_head) {}
    void add_tx(const char *broker, const char *symbol, double shares, double price, double fee, const char *side, timestamp date,
                OnDone onDone, void *caller_provided_param) {}
    void get_stock_portfolio(StockPortfolioBuilder *builder, const char *broker, const char *symbol)
    {
        // mimic the behavior of the real storage
        // see firestore::get_stock_portfolio
        builder->prepare_stock_alloc(stocks.size());
        int tx_idx = 0;
        for (const auto &stock : stocks)
        {
            builder->add_stock(stock.symbol, stock.currency);
            const auto &txs = *all_tx[tx_idx++];
            builder->prepare_tx_alloc(stock.symbol, txs.size());
            for (const auto tx : txs)
            {
                builder->incr_counter(stock.symbol);
                builder->addTx(tx.broker, stock.symbol, tx.side, tx.price, tx.shares, tx.fee, tx.date);
            }
        }
    }
};

static std::vector<stock_test_data> test1_stocks = {
    {"SYM1", "USD"},
    {"SYM2", "USD"},
};

static std::vector<stock_tx_test_data> sym1_tx = {
    {10.0, 100.0, 1.0, "BUY", "broker1", 1000},
    {0.0, 2.0, 0.0, "SPLIT", "broker1", 2000},   // total shares after split: 20
    {20.0, 40.0, 1.0, "BUY", "broker2", 3000},   // shares: 40
    {30.0, 100.0, 1.0, "SELL", "broker2", 4000}, // shares: 10
};

static std::vector<stock_tx_test_data> sym2_tx = {
    {10.0, 100.0, 1.0, "BUY", "broker1", 1000},
    {20.0, 40.0, 1.0, "BUY", "broker2", 2000},   // shares: 30
    {30.0, 100.0, 1.0, "SELL", "broker2", 3000}, // shares:  0
    {30.0, 100.0, 1.0, "BUY", "broker1", 3000},  // shares: 30
    {20.0, 40.0, 1.0, "SELL", "broker1", 4000},  // shares: 10
    {20.0, 60.0, 1.0, "BUY", "broker2", 4000},   // shares: 30
    {10.0, 70.0, 1.0, "BUY", "broker3", 4000},   // shares: 40
    {15.0, 100.0, 1.0, "SELL", "broker2", 5000}, // shares: 25
};

TEST(TestStockPortfolio, get_stock_all_portfolio1)
{
    auto triggered = false;
    static std::vector<stock_tx_test_data> *all_tx[] = {&sym1_tx, &sym2_tx};
    auto storage = Storage<MockedStocksDao>(new MockedStocksDao(test1_stocks, all_tx));
    storage.get_stock_portfolio(
        nullptr, nullptr, [](stock_portfolio *p, void *param)
        {
    bool *triggered = reinterpret_cast<bool*>(param);
    *triggered = true;
    ASSERT_EQ(p->num, test1_stocks.size());
    StockPortfolio *port = static_cast<StockPortfolio*>(p);
    int stock_idx = 0;
    for(const auto& stx: *port){
      ASSERT_STREQ(stx.instrument->symbol, test1_stocks[stock_idx].symbol.c_str());
      ASSERT_STREQ(stx.instrument->currency, test1_stocks[stock_idx].currency.c_str());

      const auto &txs = *all_tx[stock_idx++];
      auto *list = static_cast<StockTxList*>(stx.tx_list);
      int tx_idx = 0;
      for(const auto& tx: *list){
        ASSERT_STREQ(tx.broker, txs[tx_idx].broker.c_str());
        ASSERT_STREQ(tx.Side(), txs[tx_idx].side.c_str());
        ASSERT_EQ(tx.price, txs[tx_idx].price);
        ASSERT_EQ(tx.shares, txs[tx_idx].shares);
        ASSERT_EQ(tx.fee, txs[tx_idx].fee);
        ASSERT_EQ(tx.date, txs[tx_idx].date);
        ++tx_idx;
      }

      auto balance = list->calc();
      if(std::string(stx.instrument->symbol) == "SYM1"){
        // SYM1 shares/price change:
        // 10/100 => 20/50 => [20/50, 20/40] 
        //        => sold 30*100 , remains [10/40]
        const double expected_liq =  -10*100 - 20*40 + 30.0 * 100.0;
        const double expected_fee = 3.0;
        ASSERT_EQ(expected_fee, balance.fee);
        ASSERT_EQ(expected_liq, balance.liquidated);
        const double expected_shares = 10.0;
        ASSERT_EQ(expected_shares, balance.shares);
        ASSERT_EQ(40.0, balance.vwap);
      }
      else{
        // SYM2 shares/price change:
        // 10/100 => [10/100,20/40] 
        //        => sold 30*100 , remains 0
        //        => [30/100] 
        //        => sold 20*40 , remains [10/100]
        //        => [10/100, 20/60]
        //        => [10/100, 20/60, 10/70]
        //        => sold 10*100 + 5*100, remains [15/60, 10/70]
        const double expected_liq =  
          -10*100 - 20*40 + 30.0 * 100.0
          -30*100 + 20*40 - 20*60 - 10*70 + 10*100 + 5*100;
        const double expected_fee = 8.0;
        ASSERT_EQ(expected_fee, balance.fee);
        ASSERT_EQ(expected_liq, balance.liquidated);
        const double expected_shares = 15.0 + 10.0;
        ASSERT_EQ(expected_shares, balance.shares);
        ASSERT_EQ((15.0*60.0 + 10.0*70.0)/expected_shares , balance.vwap);
      }
    }
    delete port; },
        &triggered);
    ASSERT_TRUE(triggered);
}

static std::vector<stock_test_data> test2_stocks = {
    {"0700.HK", "HKD"},
};
static std::vector<stock_tx_test_data> test2_tx = {
    {100.0, 369.0, 72.75, "BUY", "broker1", 1000},
    {100.0, 260.0, 51.302, "BUY", "broker1", 2000},
    {100.0, 468.6, 99.76, "BUY", "broker1", 3000},
    {100.0, 373.0, 79.90, "BUY", "broker1", 4000},
};

TEST(TestStockPortfolio, get_stock_all_portfolio2)
{
    auto triggered = false;
    static std::vector<stock_tx_test_data> *all_tx[] = {&test2_tx};
    auto storage = Storage<MockedStocksDao>(new MockedStocksDao(test2_stocks, all_tx));
    storage.get_stock_portfolio(
        nullptr, nullptr, [](stock_portfolio *p, void *param)
        {
    bool *triggered = reinterpret_cast<bool*>(param);
    *triggered = true;
    StockPortfolio *port = static_cast<StockPortfolio*>(p);
    for(const auto& stx: *port){
      auto *list = static_cast<StockTxList*>(stx.tx_list);
      auto balance = list->calc();
      ASSERT_EQ(balance.shares, 400 );
      ASSERT_EQ(balance.vwap, 367.65);
      break;
    }
    delete port; },
        &triggered);
    ASSERT_TRUE(triggered);
}

static const char usd[] = "USD";
static const char jpy[] = "JPY";
static const std::string usd_jpy = {"USDJPY"};
static const double usd_jpy_rate = 100;
static const timestamp usd_jpy_date = 100;

static const double fee = 1.0;

static const std::string broker1 = {"broker1"};
static const std::string broker2 = {"broker2"};
static const std::string* brokers[] = {&broker1, &broker2};

static double broker1_jpy = 10000;
static double broker1_usd = 1000;

static double broker2_jpy = 20000;
static double broker2_usd = 2000;

static const std::string stock_both_brokers = {"Stock4"};
static const char* stock_both_brokers_ccy = usd;
static const double stock_both_brokers_price = 300;
static const timestamp stock_both_brokers_date = 100;

static const double stock_both_brokers_broker1_buy_price = 90;
static const double stock_both_brokers_broker1_shares = 100;

static const double stock_both_brokers_broker2_buy_price = 95;
static const double stock_both_brokers_broker2_shares = 50;

static const double stock_both_brokers_broker1_buy_price2 = 90;
static const double stock_both_brokers_broker1_shares2 = 60;


static const std::string stock1 = {"Stock1"};
static const std::string& stock1_broker = broker1;
static const char* stock1_ccy = usd;
static const double stock1_price = 100;
static const double stock1_buy_price = 90;
static const double stock1_shares = 100;
static const timestamp stock1_date = 100;


static const std::string stock2 = {"Stock2"};
static const std::string& stock2_broker = broker2;
static const char* stock2_ccy = jpy;
static const double stock2_price = 200;
static const double stock2_buy_price = 190;
static const double stock2_shares = 100;
static const timestamp stock2_date = 100;

static const std::string stock3 = {"Stock3"};
static const std::string& stock3_broker = broker2;
static const char* stock3_ccy = usd;
static const double stock3_price = 30;
static const double stock3_buy_price = 10;
static const double stock3_shares = 10;
static const timestamp stock3_date = 100;


static const std::string funds1 = {"Funds1"};
static const std::string funds1_id = {"id1"};
static const std::string& funds1_broker = broker2;
static const double funds1_amt = 1;
static const double funds1_price = 1000;
static const double funds1_value = 3000;
static const double funds1_capital = 2000;
static const double funds1_profit = funds1_value - funds1_capital;
static const double funds1_roi = 100 * funds1_profit/funds1_capital;
static const timestamp funds1_date = 100;

static const std::string funds2 = {"Funds2"};
static const std::string funds2_id = {"id2"};
static const std::string& funds2_broker = broker1;
static const double funds2_amt = 2;
static const double funds2_price = 900;
static const double funds2_value = 4000;
static const double funds2_capital = 5000;
static const double funds2_profit = funds2_value - funds2_capital;
static const double funds2_roi = 100 * funds2_profit/funds2_capital;
static const timestamp funds2_date = 100;

static const std::string funds3 = {"Funds3"};
static const std::string funds3_id = {"id3"};
static const std::string& funds3_broker = broker1;
static const double funds3_amt = 3;
static const double funds3_price = 900;
static const double funds3_value = 4000;
static const double funds3_capital = 5000;
static const double funds3_profit = funds3_value - funds3_capital;
static const double funds3_roi = 100 * funds3_profit/funds3_capital;
static const timestamp funds3_date = 100;


static Quotes* prepare_quotes(QuoteBySymbol& quotes_by_symbol)
{
    Quotes *q = nullptr;
    auto *builder = static_cast<LatestQuotesBuilder *>(LatestQuotesBuilder::create(4, [&q](LatestQuotesBuilder::Alloc *alloc){
        q = new Quotes(alloc->allocated_num(), alloc->head);
    }));
    builder->add_quote(usd_jpy, usd_jpy_date, usd_jpy_rate);
    builder->add_quote(stock1, stock1_date, stock1_price);
    builder->add_quote(stock2, stock2_date, stock2_price);
    builder->add_quote(stock3, stock3_date, stock3_price);
    builder->add_quote(stock_both_brokers, stock_both_brokers_date, stock_both_brokers_price);
    builder->succeed();

    for(auto const& quote: *q){
        quotes_by_symbol[quote.symbol] = &quote;
    }
 
    return q;
}

static StockPortfolio* prepare_stocks()
{
    StockPortfolio* stocks;

    auto *builder = StockPortfolioBuilder::create([&stocks](StockPortfolioBuilder::StockAlloc* stock_alloc, const StockPortfolioBuilder::TxAllocPointerBySymbol& tx){
        const auto stock_num = stock_alloc->allocated_num();
        auto *stock_with_tx_head = StockPortfolioBuilder::create_stock_with_tx(stock_alloc, tx);
        stocks = new StockPortfolio(stock_num, stock_alloc->head, stock_with_tx_head);
    });

    builder->prepare_stock_alloc(4);
    const std::string side = {"BUY"};

    builder->add_stock(stock1, stock1_ccy);
    builder->prepare_tx_alloc(stock1, 1);
    builder->incr_counter(stock1);
    builder->addTx(stock1_broker, stock1, side, stock1_buy_price, stock1_shares, fee, stock1_date);

    builder->add_stock(stock2, stock2_ccy);
    builder->prepare_tx_alloc(stock2, 1);
    builder->incr_counter(stock2);
    builder->addTx(stock2_broker, stock2, side, stock2_buy_price, stock2_shares, fee, stock2_date);

    builder->add_stock(stock_both_brokers, stock_both_brokers_ccy);
    builder->prepare_tx_alloc(stock_both_brokers, 3);
    builder->incr_counter(stock_both_brokers);
    builder->addTx(broker1, stock_both_brokers, side, stock_both_brokers_broker1_buy_price, stock_both_brokers_broker1_shares, fee, stock_both_brokers_date);
    builder->incr_counter(stock_both_brokers);
    builder->addTx(broker2, stock_both_brokers, side, stock_both_brokers_broker2_buy_price, stock_both_brokers_broker2_shares, fee, stock_both_brokers_date);
    builder->incr_counter(stock_both_brokers);
    builder->addTx(broker1, stock_both_brokers, side, stock_both_brokers_broker1_buy_price2, stock_both_brokers_broker1_shares2, fee, stock_both_brokers_date);

    builder->add_stock(stock3, stock3_ccy);
    builder->prepare_tx_alloc(stock3, 1);
    builder->incr_counter(stock3);
    builder->addTx(stock3_broker, stock3, side, stock3_buy_price, stock3_shares, fee, stock3_date);

    return stocks;
}

static FundPortfolio* prepare_funds()
{
    FundPortfolio* funds;
    auto *builder = static_cast<FundsBuilder*>(FundsBuilder::create(3,[&funds](FundsBuilder::Alloc* fund_alloc){
        funds = new FundPortfolio(fund_alloc->allocated_num(), fund_alloc->head);
    }));

    builder->add_fund(funds1_broker, funds1, funds1_id, funds1_amt, funds1_capital, funds1_value, funds1_price, funds1_profit, funds1_roi, funds1_date);
    builder->add_fund(funds2_broker, funds2, funds2_id, funds2_amt, funds2_capital, funds2_value, funds2_price, funds2_profit, funds2_roi, funds2_date);
    builder->add_fund(funds3_broker, funds3, funds3_id, funds3_amt, funds3_capital, funds3_value, funds3_price, funds3_profit, funds3_roi, funds3_date);
    builder->succeed();
    return funds;
}

typedef int BrokerType;
class BrokerDao
{
public:

    std::string get_broker_name(const BrokerType& broker){
        return *brokers[broker];
    }
    BrokerBuilder get_broker_cash_balance_and_active_funds(const BrokerType& broker){
        BrokerBuilder builder(2, broker == 0 ? 2:1);
        if(broker == 0){
            //broker1       
            builder.add_active_fund(funds2_id);
            builder.add_active_fund(funds3_id);
            builder.add_cash_balance(usd, broker1_usd);
            builder.add_cash_balance(jpy, broker1_jpy);
        }
        else{
            //broker2
            builder.add_active_fund(funds1_id);
            builder.add_cash_balance(usd, broker2_usd);
            builder.add_cash_balance(jpy, broker2_jpy);
        }
        return builder;
    }
};

static AllBrokers* prepare_brokers()
{
    BrokerDao dao;
    auto *builder = new AllBrokerBuilder<BrokerDao, BrokerType>(2);
    builder->add_broker(&dao, 0);
    builder->add_broker(&dao, 1);
    AllBrokers * b = new AllBrokers(builder->broker_num, builder->head);
    delete builder;
    return b;
}

static const char ASSET_TYPE_STOCK [] = "Stock&ETF";
static const char ASSET_TYPE_FUNDS [] = "Funds";
static const char ASSET_TYPE_CASH  [] = "Cash";

TEST(TestOverview, load_assets)
{
    QuoteBySymbol quotes_by_symbol;
    auto * q = prepare_quotes(quotes_by_symbol);
    auto * funds = prepare_funds();
    auto * brokers = prepare_brokers();
    auto * stocks = prepare_stocks();

    auto* assets = new AllAssets(std::move(quotes_by_symbol), brokers, funds, stocks);


    AssetItems items = {
        AssetItem(ASSET_TYPE_CASH , broker1.c_str(), usd, broker1_usd, 0),
        AssetItem(ASSET_TYPE_CASH , broker1.c_str(), jpy, broker1_jpy, 0),
        AssetItem(ASSET_TYPE_CASH , broker2.c_str(), usd, broker2_usd, 0),
        AssetItem(ASSET_TYPE_CASH , broker2.c_str(), jpy, broker2_jpy, 0),

        AssetItem(ASSET_TYPE_FUNDS, funds2_broker.c_str(), jpy, funds2_value + funds3_value, funds2_profit + funds3_profit),
        AssetItem(ASSET_TYPE_FUNDS, funds1_broker.c_str(), jpy, funds1_value, funds1_profit),

        AssetItem(ASSET_TYPE_STOCK, broker1.c_str(), usd, 
            // stock1 + stock4
            stock1_price * stock1_shares + stock_both_brokers_price * (stock_both_brokers_broker1_shares + stock_both_brokers_broker1_shares2),
            (stock1_price - stock1_buy_price) * stock1_shares 
                + (stock_both_brokers_price - stock_both_brokers_broker1_buy_price)  * stock_both_brokers_broker1_shares
                + (stock_both_brokers_price - stock_both_brokers_broker1_buy_price2) * stock_both_brokers_broker1_shares2
        ),
        AssetItem(ASSET_TYPE_STOCK, broker2.c_str(), jpy, 
            // stock2
            stock2_price * stock2_shares,
            (stock2_price - stock2_buy_price) * stock2_shares
        ),
        AssetItem(ASSET_TYPE_STOCK, broker2.c_str(), usd, 
            // stock3 + stock4
            stock3_price * stock3_shares + stock_both_brokers_price * stock_both_brokers_broker2_shares,
            (stock3_price - stock3_buy_price) * stock3_shares + (stock_both_brokers_price - stock_both_brokers_broker2_buy_price) * stock_both_brokers_broker2_shares
        ),
    };

    ASSERT_EQ(items,assets->items);

    delete brokers;
    delete funds;
    delete stocks;
    delete q;
    delete assets;
}

static inline bool cstr_eq(const char*s1, const char*s2)
{
    return strcmp(s1, s2) == 0;
}

static inline bool cmp_overview_item(overview_item& a, overview_item& b)
{
    return cstr_eq(a.name, b.name) && 
        a.profit == b.profit && a.profit_in_main_ccy == b.profit_in_main_ccy &&
        a.value == b.value && a.value_in_main_ccy == b.value_in_main_ccy;
}

static void assert_overview_items_eq(std::vector<overview_item>& items, std::vector<overview_item>& expected)
{
    ASSERT_EQ(items.size(), expected.size());
    auto i = items.begin();
    auto k = expected.begin();
    while(i!=items.end()){
        ASSERT_TRUE(cmp_overview_item(*i++, *k++));
    }
}

extern overview* get_overview(AllAssets* assets, const char* main_ccy, GROUP level1_group, GROUP level2_group, GROUP level3_group);
TEST(TestOverview, overview_group_by_asset_broker)
{
    QuoteBySymbol quotes_by_symbol;
    auto * q = prepare_quotes(quotes_by_symbol);
    auto * funds = prepare_funds();
    auto * brokers = prepare_brokers();
    auto * stocks = prepare_stocks();

    auto* assets = new AllAssets(std::move(quotes_by_symbol), brokers, funds, stocks);

    auto* overview = static_cast<Overview*>(get_overview(assets, jpy, GROUP_BY_ASSET, GROUP_BY_BROKER, GROUP_BY_CCY));

    std::vector<overview_item> broker1_cash = {
        overview_item{ const_cast<char*>(usd), broker1_usd, broker1_usd * usd_jpy_rate , 0,0 },
        overview_item{ const_cast<char*>(jpy), broker1_jpy, broker1_jpy                , 0,0 },
    };
    std::vector<overview_item> broker2_cash = {
        overview_item{ const_cast<char*>(usd), broker2_usd, broker2_usd * usd_jpy_rate , 0,0 },
        overview_item{ const_cast<char*>(jpy), broker2_jpy, broker2_jpy                , 0,0 },
    };

    std::vector<overview_item> broker1_funds = {
        overview_item{ const_cast<char*>(jpy), 
            funds2_value  + funds3_value , funds2_value  + funds3_value, 
            funds2_profit + funds3_profit, funds2_profit + funds3_profit
        },
    };
    std::vector<overview_item> broker2_funds = {
        overview_item{ const_cast<char*>(jpy), funds1_value, funds1_value, funds1_profit, funds1_profit},
    };

    double broker1_stocks_value = 
        stock1_price * stock1_shares + 
        stock_both_brokers_price * stock_both_brokers_broker1_shares +
        stock_both_brokers_price * stock_both_brokers_broker1_shares2;

    double broker1_stocks_profit = 
        (stock1_price - stock1_buy_price) * stock1_shares + 
        (stock_both_brokers_price - stock_both_brokers_broker1_buy_price ) * stock_both_brokers_broker1_shares +
        (stock_both_brokers_price - stock_both_brokers_broker1_buy_price2) * stock_both_brokers_broker1_shares2;

    std::vector<overview_item> broker1_stock = {
        overview_item{
            const_cast<char*>(usd), 
            broker1_stocks_value,  broker1_stocks_value  * usd_jpy_rate, 
            broker1_stocks_profit, broker1_stocks_profit * usd_jpy_rate
        } 
    };

    double broker2_usd_stocks_value = 
        stock3_price * stock3_shares +
        stock_both_brokers_price * stock_both_brokers_broker2_shares;

    double broker2_usd_stocks_profit = 
        (stock3_price - stock3_buy_price) * stock3_shares + 
        (stock_both_brokers_price - stock_both_brokers_broker2_buy_price) * stock_both_brokers_broker2_shares;

    double broker2_jpy_stocks_value =  stock2_price * stock2_shares;
    double broker2_jpy_stocks_profit = (stock2_price - stock2_buy_price) * stock2_shares;
    std::vector<overview_item> broker2_stock = {
       // jpy
        overview_item{
            const_cast<char*>(jpy), 
            broker2_jpy_stocks_value, broker2_jpy_stocks_value,
            broker2_jpy_stocks_profit, broker2_jpy_stocks_profit
        },
        // usd
        overview_item{
            const_cast<char*>(usd), 
            broker2_usd_stocks_value,  broker2_usd_stocks_value  * usd_jpy_rate, 
            broker2_usd_stocks_profit, broker2_usd_stocks_profit * usd_jpy_rate
        },
    };

    double broker1_cash_value =  broker1_jpy + broker1_usd * usd_jpy_rate;
    double broker2_cash_value =  broker2_jpy + broker2_usd * usd_jpy_rate;

    double broker1_stock_value = broker1_stocks_value * usd_jpy_rate;
    double broker2_stock_value = broker2_jpy_stocks_value + broker2_usd_stocks_value * usd_jpy_rate;
    double broker1_stock_profit = broker1_stocks_profit * usd_jpy_rate;
    double broker2_stock_profit = broker2_jpy_stocks_profit + broker2_usd_stocks_profit * usd_jpy_rate;

    auto add_funds_callback = [](std::pair<double,double>& a,overview_item& x){
        a.first += x.value_in_main_ccy;
        a.second += x.profit_in_main_ccy;
        return a;
    };
    auto broker1_funds_value_profit = std::accumulate(broker1_funds.begin(), broker1_funds.end(), std::make_pair(0.0,0.0), add_funds_callback);
    auto broker2_funds_value_profit = std::accumulate(broker2_funds.begin(), broker2_funds.end(), std::make_pair(0.0,0.0), add_funds_callback);

    for(auto& lvl1: *overview){
        for(auto& lvl2: lvl1){
            // don't use OverviewItem : 
            // vector would create and destroy temp item objects, if OverviewItem is used its char* member will be freed
            std::vector<overview_item> items;
            std::copy(lvl2.begin(), lvl2.end(), std::back_inserter(items));
            if(cstr_eq(lvl1.name, ASSET_TYPE_CASH)){
                std::vector<overview_item>& expected = cstr_eq(lvl2.name, "broker1") ? broker1_cash : broker2_cash;
                assert_overview_items_eq(items, expected);

                double expected_value =  cstr_eq(lvl2.name, "broker1") ? broker1_cash_value : broker2_cash_value;
                ASSERT_EQ(lvl2.value_sum_in_main_ccy,  expected_value);
                ASSERT_EQ(lvl2.profit_sum_in_main_ccy, 0);
                ASSERT_EQ(lvl1.value_sum_in_main_ccy,  broker1_cash_value + broker2_cash_value);
                ASSERT_EQ(lvl1.profit_sum_in_main_ccy, 0);
           }
            else if(cstr_eq(lvl1.name, ASSET_TYPE_FUNDS)){
                std::vector<overview_item>& expected = cstr_eq(lvl2.name, "broker1") ? broker1_funds: broker2_funds;
                assert_overview_items_eq(items, expected);

                double expected_value = cstr_eq(lvl2.name, "broker1") ? broker1_funds_value_profit.first : broker2_funds_value_profit.first;
                double expected_profit = cstr_eq(lvl2.name, "broker1") ? broker1_funds_value_profit.second : broker2_funds_value_profit.second;
                ASSERT_EQ(lvl2.value_sum_in_main_ccy, expected_value);
                ASSERT_EQ(lvl2.profit_sum_in_main_ccy, expected_profit);
                ASSERT_EQ(lvl1.value_sum_in_main_ccy, broker1_funds_value_profit.first + broker2_funds_value_profit.first);
                ASSERT_EQ(lvl1.profit_sum_in_main_ccy, broker1_funds_value_profit.second + broker2_funds_value_profit.second);
            }
            else if(cstr_eq(lvl1.name, ASSET_TYPE_STOCK)){
                std::vector<overview_item>& expected = cstr_eq(lvl2.name, "broker1") ? broker1_stock: broker2_stock;
                assert_overview_items_eq(items, expected);

                double expected_value =  cstr_eq(lvl2.name, "broker1") ? broker1_stock_value : broker2_stock_value ;
                double expected_profit = cstr_eq(lvl2.name, "broker1") ? broker1_stock_profit : broker2_stock_profit;
                ASSERT_EQ(lvl2.value_sum_in_main_ccy, expected_value);
                ASSERT_EQ(lvl2.profit_sum_in_main_ccy, expected_profit);
                ASSERT_EQ(lvl1.value_sum_in_main_ccy, broker1_stock_value + broker2_stock_value);
                ASSERT_EQ(lvl1.profit_sum_in_main_ccy, broker1_stock_profit + broker2_stock_profit);
            }
            else ASSERT_TRUE(false);
        }
    }

    delete brokers;
    delete funds;
    delete stocks;
    delete q;
    delete assets;
}