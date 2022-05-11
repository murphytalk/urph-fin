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

class MockedStocksDao
{
public:
  MockedStocksDao(OnInitDone){
  }
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

  }
};

TEST(TestStockPortfolio, calculate)
{
  auto storage = Storage<MockedStocksDao>(nullptr);
}