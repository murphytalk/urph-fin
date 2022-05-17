#include <gtest/gtest.h>

#include "core/stock.hxx"
#include "storage/storage.hxx"

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

struct stock_test_data{
  std::string symbol;
  std::string currency;
};

struct stock_tx_test_data{
  double shares;
  double price;
  double fee;
  std::string side;
  std::string broker;
  timestamp date;
};

static std::vector<stock_test_data> stocks = {
  {"SYM1", "USD"},
  {"SYM2", "USD"},
};

static std::vector<stock_tx_test_data> sym1_tx = {
  {10.0, 100.0, 1.0, "BUY"  , "broker1", 1000},
  { 0.0,   2.0, 0.0, "SPLIT", "broker1", 2000}, // total shares after split: 20
  {20.0,  40.0, 1.0, "BUY"  , "broker2", 3000}, // shares: 40
  {30.0, 100.0, 1.0, "SELL" , "broker2", 4000}, // shares: 10
};

static std::vector<stock_tx_test_data> sym2_tx = {
  {10.0, 100.0, 1.0, "BUY"  , "broker1", 1000},
  {20.0,  40.0, 1.0, "BUY"  , "broker2", 2000}, // shares: 30
  {30.0, 100.0, 1.0, "SELL" , "broker2", 3000}, // shares:  0
  {30.0, 100.0, 1.0, "BUY"  , "broker1", 3000}, // shares: 30
  {20.0,  40.0, 1.0, "SELL" , "broker1", 4000}, // shares: 10
  {20.0,  60.0, 1.0, "BUY"  , "broker2", 4000}, // shares: 30
  {10.0,  70.0, 1.0, "BUY"  , "broker3", 4000}, // shares: 40
  {15.0, 100.0, 1.0, "SELL" , "broker2", 5000}, // shares: 25
};

static std::vector<stock_tx_test_data>* all_tx[] = {&sym1_tx, &sym2_tx};
class MockedStocksDao
{
public:
  MockedStocksDao(OnInitDone){}
  typedef int BrokerType;
  BrokerType get_broker_by_name(const char *) { return 0; }
  std::string get_broker_name(const BrokerType &) { return std::string(""); }
  BrokerBuilder get_broker_cash_balance_and_active_funds(const BrokerType &)
  {
    return BrokerBuilder(0, 0);
  }
  AllBrokerBuilder<MockedStocksDao, BrokerType> *get_brokers() { return nullptr; }
  StringsBuilder *get_all_broker_names(size_t &) { return nullptr; }
  void get_funds(FundsBuilder *, int, const char **) {}
  StringsBuilder get_known_stocks(const char*) { return StringsBuilder(0); }

  void get_stock_portfolio(StockPortfolioBuilder* builder, const char* broker, const char* symbol){
    // mimic the behavior of the real storage
    // see firestore::get_stock_portfolio
    builder->prepare_stock_alloc(stocks.size());
    int tx_idx = 0;
    for(const auto& stock: stocks){
      builder->add_stock(stock.symbol, stock.currency);
      const auto &txs = *all_tx[tx_idx++];
      builder->prepare_tx_alloc(stock.symbol, txs.size());
      for(const auto tx: txs){
        builder->incr_counter(stock.symbol);
        builder->addTx(tx.broker, stock.symbol, tx.side, tx.price, tx.shares, tx.fee, tx.date);
      }
    }
  }
};

TEST(TestStockPortfolio, get_stock_all_portfolio)
{
  auto triggered = false;
  auto storage = Storage<MockedStocksDao>(nullptr);
  storage.get_stock_portfolio(nullptr, nullptr, [](stock_portfolio* p, void* param){
    bool *triggered = reinterpret_cast<bool*>(param);
    *triggered = true;
    ASSERT_EQ(p->num, stocks.size());
    StockPortfolio *port = static_cast<StockPortfolio*>(p);
    int stock_idx = 0;
    for(const auto& stx: *port){
      ASSERT_STREQ(stx.instrument->symbol, stocks[stock_idx].symbol.c_str());
      ASSERT_STREQ(stx.instrument->currency, stocks[stock_idx].currency.c_str());

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
    delete port;
  },&triggered);
  ASSERT_TRUE(triggered);
}