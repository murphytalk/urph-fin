#include "urph-fin-core.h"
#include <string>

// C++ extentions to make life easier in C++

// the ground policy is that none of the extended C++ classes should add member variables
// so the size of the class remain the same as the original struct
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
