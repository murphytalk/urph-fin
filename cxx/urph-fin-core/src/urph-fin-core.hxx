#include "urph-fin-core.h"
#include <string>
#include <vector>

// C++ extentions to make life easier for C++ clients

class CashBalance: public cash_balance{
public:
    CashBalance(const std::string& n, float v);
    ~CashBalance();
    std::string to_str();
};

class Broker: public broker{
public:
    Broker(const std::string&n, int ccy_num, cash_balance* first_ccy_balance);
    ~Broker();
    CashBalance* head();
    std::string to_str();
};

class AllBrokers: public all_brokers{
public:
    AllBrokers();
    AllBrokers(int n, broker* broker);
    ~AllBrokers();
    Broker* head();
    std::string to_str();
};

// the ground policy is that none of the extended C++ classes should add member variables
// so the size of the class remain the same as the original struct

static_assert(sizeof(AllBrokers) == sizeof(all_brokers));
static_assert(sizeof(Broker) == sizeof(broker));
static_assert(sizeof(CashBalance)  == sizeof(cash_balance));
