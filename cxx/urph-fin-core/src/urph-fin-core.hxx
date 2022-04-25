#ifndef URPH_FIN_CORE_HXX_
#define URPH_FIN_CORE_HXX_

#include "urph-fin-core.h"
#include <string>
#include <vector>
#include <iterator> 

// C++ extentions to make life (much more) easier for C++ clients
template<typename T> struct Iterator
{
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = T;
    using pointer           = value_type*;
    using reference         = value_type&;

    Iterator(pointer ptr) : _ptr(ptr) {}

    reference operator*() const { return *_ptr; }
    pointer operator->() { return _ptr; }

    // Prefix increment
    Iterator& operator++() { _ptr++; return *this; }  

    // Postfix increment
    Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }

    friend bool operator== (const Iterator& a, const Iterator& b) { return a._ptr == b._ptr; };
    friend bool operator!= (const Iterator& a, const Iterator& b) { return a._ptr != b._ptr; };     
private:
    pointer _ptr;
};

// tag dispatching - to help free_multiple_structs() select the right head/size
struct member_tag {};
struct first_member_tag  : public member_tag {};
struct second_member_tag : public member_tag{};

template<typename Wrapper, typename T, typename MemberTag=first_member_tag>
void free_multiple_structs(Wrapper* data, MemberTag mt = MemberTag()){
    T* p = data->head(mt);
    for(int i = 0; i < data->size(mt); ++i, ++p){
        p->~T();
    }
}

class CashBalance: public cash_balance{
public:
    CashBalance(const std::string& n, float v);
    ~CashBalance();
};

class ActiveFund: public active_fund{
public:
    ActiveFund(const std::string& i);
    ~ActiveFund();
};

class Broker: public broker{
public:
    Broker(const std::string&n, int ccy_num, cash_balance* first_ccy_balance, int active_funds_num, active_fund* first_fund);
    ~Broker();

    inline CashBalance* head(first_member_tag) { return static_cast<CashBalance*>(first_cash_balance); }
    inline int size(first_member_tag) { return num; } 
    Iterator<CashBalance> begin() { return Iterator( head(first_member_tag()) ); }
    Iterator<CashBalance> end() { return Iterator( head(first_member_tag()) + num ); }

    inline ActiveFund* head(second_member_tag) { return static_cast<ActiveFund*>(first_active_fund); }
    inline int size(second_member_tag) { return active_funds_num; }
    Iterator<ActiveFund> fund_begin() { return Iterator( head(second_member_tag()) ); }
    Iterator<ActiveFund> fund_end() { return Iterator( head(second_member_tag()) + active_funds_num); }
};

class AllBrokers: public all_brokers{
public:
    AllBrokers(int n, broker* broker);
    ~AllBrokers();
    inline Broker* head(first_member_tag) { return static_cast<Broker*>(first_broker); }
    inline int size(first_member_tag) { return num; } 
    Iterator<Broker> begin() { return Iterator( head(first_member_tag()) ); }
    Iterator<Broker> end() { return Iterator( head(first_member_tag()) + num ); }
};

class Fund: public fund{
public:
    Fund(const char* b, const char* n, const char* i, int a, double c, double m, double p, timestamp d)
    {
        broker = b;
        name = n;
        id = i;
        amount = a;
        capital = c;
        market_value = m;
        price = p;
        date = d;
    }    
};

class FundPortfolio: public fund_portfolio{
public:    
    FundPortfolio(int n, fund* fund);
    ~FundPortfolio();
    inline Fund* head(first_member_tag) { return static_cast<Fund*>(first_fund); }
    inline int size(first_member_tag) { return num; }

    Iterator<Fund> begin() { return Iterator( head(first_member_tag()) ); }
    Iterator<Fund> end() { return Iterator( head(first_member_tag()) + num ); }
};

// the ground rule is that none of the extended C++ classes should add member variables
// so the size of the class remain the same as the original struct
static_assert(sizeof(AllBrokers) == sizeof(all_brokers));
static_assert(sizeof(Broker) == sizeof(broker));
static_assert(sizeof(CashBalance)  == sizeof(cash_balance));
static_assert(sizeof(Fund)  == sizeof(fund));
static_assert(sizeof(FundPortfolio)  == sizeof(fund_portfolio));

#endif
