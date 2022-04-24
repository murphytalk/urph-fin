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

class CashBalance: public cash_balance{
public:
    CashBalance(const std::string& n, float v);
    ~CashBalance();
};

class Broker: public broker{
public:
    Broker(const std::string&n, int ccy_num, cash_balance* first_ccy_balance);
    ~Broker();
    CashBalance* head();
    Iterator<CashBalance> begin() { return Iterator( head() ); }
    Iterator<CashBalance> end() { return Iterator( head() + num ); }
};

class AllBrokers: public all_brokers{
public:
    AllBrokers(int n, broker* broker);
    ~AllBrokers();
    Broker* head();
    Iterator<Broker> begin() { return Iterator( head() ); }
    Iterator<Broker> end() { return Iterator( head() + num ); }
};

// the ground rule is that none of the extended C++ classes should add member variables
// so the size of the class remain the same as the original struct
static_assert(sizeof(AllBrokers) == sizeof(all_brokers));
static_assert(sizeof(Broker) == sizeof(broker));
static_assert(sizeof(CashBalance)  == sizeof(cash_balance));

#endif
