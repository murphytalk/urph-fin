#include <gtest/gtest.h>

#include "core/stock.hxx"

TEST(TestStock, Basic)
{
  Stock s1(std::string("Broker"), std::string("SYM"), std::string("USD"));
  stock *s2 = &s1;
  // Expect equality.
  ASSERT_STREQ(s2->broker, "Broker");
  ASSERT_STREQ(s2->symbol, "SYM");
  ASSERT_STREQ(s2->currency, "USD");
}

TEST(TestStock, MoveAssignment)
{
  Stock s1(std::string("Broker1"), std::string("SYM"), std::string("USD"));
  Stock s2(std::string("Broker2"), std::string("NOS"), std::string("JPY"));
  Stock *p = &s2;
  *p = std::move(s1);
  ASSERT_STREQ(s2.broker, "Broker1");
  ASSERT_STREQ(s2.symbol, "SYM");
  ASSERT_STREQ(s2.currency, "USD");
  ASSERT_EQ(s1.broker, nullptr);
  ASSERT_EQ(s1.symbol, nullptr);
  ASSERT_EQ(s1.currency, nullptr);
}

TEST(TestStockWithTx, MoveAssignment)
{
  Stock s1(std::string("Broker1"), std::string("SYM"), std::string("USD"));
  StockTx tx(0.0, 0.1, 0.2, std::string("BUY"));
  StockTxList txList(1, &tx);

  ASSERT_EQ(txList.num, 1);
  ASSERT_EQ(txList.first_tx, &tx);

  StockWithTx x(s1, txList);

  ASSERT_STREQ(x.instrument.broker, "Broker1");
  ASSERT_STREQ(x.instrument.symbol, "SYM");
  ASSERT_STREQ(x.instrument.currency, "USD");
  ASSERT_EQ(x.tx_list.num, 1);
  ASSERT_EQ(x.tx_list.first_tx, &tx);

  ASSERT_EQ(s1.broker, nullptr);
  ASSERT_EQ(s1.symbol, nullptr);
  ASSERT_EQ(s1.currency, nullptr);
  ASSERT_EQ(txList.num, 0);
  ASSERT_EQ(txList.first_tx, nullptr);
}