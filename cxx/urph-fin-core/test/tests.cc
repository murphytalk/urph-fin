#include <gtest/gtest.h>

#include "core/stock.hxx"

TEST(TestStock, Basic) {
  Stock s1(std::string("Broker"), std::string("SYM"), std::string("USD"));
  stock* s2 = &s1;
  // Expect equality.
  ASSERT_STREQ(s2->broker, "Broker");
  ASSERT_STREQ(s2->symbol, "SYM");
  ASSERT_STREQ(s2->currency, "USD");
}