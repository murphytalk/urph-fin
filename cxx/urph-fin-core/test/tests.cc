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
  auto storage = Storage<MockedStocksDao>(nullptr);
  storage.get_stock_portfolio(nullptr, nullptr, [](stock_portfolio* p, void* param){
    ASSERT_EQ(p->num, stocks.size());
    StockPortfolio *port = static_cast<StockPortfolio*>(p);
    int stock_idx = 0;
    for(const auto& stx: *port){
      ASSERT_STREQ(stx.instrument->symbol, stocks[stock_idx].symbol.c_str());
      ASSERT_STREQ(stx.instrument->currency, stocks[stock_idx].currency.c_str());
      /*
      int tx_idx = 0;
      StockTxList *list = static_cast<StockTxList*>(stx.tx_list);
      for(const auto& tx: *list){
        ASSERT_EQ(tx.broker, sym1_tx[tx_idx].broker);
        ASSERT_EQ(tx.side, sym1_tx[tx_idx].side);
        ASSERT_EQ(tx.price, sym1_tx[tx_idx].price);
        ASSERT_EQ(tx.shares, sym1_tx[tx_idx].shares);
        ASSERT_EQ(tx.fee, sym1_tx[tx_idx].fee);
        ASSERT_EQ(tx.date, sym1_tx[tx_idx].date);
        tx_idx++;
      }
      */
      stock_idx++; 
    }
    delete port;
  },nullptr);
}