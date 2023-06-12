#ifndef URPH_FIN_CORE_HXX_
#define URPH_FIN_CORE_HXX_

#include "urph-fin-core.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <iterator> 
#include <functional>
#include <memory>
#include <cmath>

#include "../utils.hxx"

// C++ extensions to make life (much more) easier
#ifdef USE_FIREBASE
struct Config{
    std::string email;
    std::string password;       
};

Config load_cfg();
#endif

char* copy_str(const std::string& str);
char* copy_str(const std::string_view& str);

struct NonCopyableMoveable {
    NonCopyableMoveable & operator=(const NonCopyableMoveable&) = delete;
    NonCopyableMoveable(const NonCopyableMoveable&) = delete;
    NonCopyableMoveable(NonCopyableMoveable&&) = delete;
    NonCopyableMoveable() = default;
};

template<typename T> 
class PlacementNew: public NonCopyableMoveable{
    T* _head;
    T* _current;
    int _counter = 0;
    int _max_counter;
    float _ratio;
public:    
    PlacementNew(int max_num, float increment_ratio = 0.5):
        _max_counter(max_num),
        _head(new T[max_num]),
        _current(_head),
        _ratio(1+increment_ratio){}

    inline long allocated_num() const { return _current - _head;}
    inline T* end() const{ return _current;}
    inline bool has_enough_counter() const { return allocated_num() >= _max_counter;}
    inline int inc_counter() { return allocated_num(); }
    inline T* head() const { return _head; }
    inline int counter() const { return allocated_num(); }
    inline int max_counter() const { return _max_counter; }
    T* next(){
        if(allocated_num() >= _max_counter){
            //resize
            int new_max = std::ceil(_max_counter * _ratio);
            auto* p = new T[new_max];
            memcpy(p, _head, sizeof(T) * _max_counter);
            
            delete []_head;
            _head = p;
            _current = p + _max_counter;
            _max_counter = new_max;

            return _current++;
        }
        else return _current++;
    }
};


template<typename T> struct Iterator
{
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = T;
    using pointer           = value_type*;
    using reference         = value_type&;

    explicit Iterator(pointer ptr) : _ptr(ptr) {}

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


template<typename T> struct PtrIterator
{
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = T;


    explicit PtrIterator(T ptr) : _ptr(ptr) {}

    T  operator*() const { return _ptr; }
    T* operator->() { return &_ptr; }

    // Prefix increment
    PtrIterator& operator++() { _ptr++; return *this; }  

    // Postfix increment
    PtrIterator operator++(int) { PtrIterator tmp = *this; ++(*this); return tmp; }

    friend bool operator== (const PtrIterator& a, const PtrIterator& b) { return a._ptr == b._ptr; };
    friend bool operator!= (const PtrIterator& a, const PtrIterator& b) { return a._ptr != b._ptr; };     
private:
    T _ptr;
};

// tag dispatching - to help free_multiple_structs() select the right head/size
struct default_member_tag {};

template<typename Wrapper, typename T, typename MemberTag=default_member_tag>
void free_placement_allocated_structs(Wrapper* data, MemberTag mt = MemberTag()){
    T* p = data->head(mt);
    if(p == nullptr) return;
    for(int i = 0; i < data->size(mt); ++i, ++p){
        p->~T();
    }
}

class CashBalance: public cash_balance{
public:
    CashBalance(const std::string_view& n, float v);
    ~CashBalance();
};

class Strings: public strings{
public:
    explicit Strings(int n);
    ~Strings();
    void add(const std::string_view& s, float increment_ratio = 0.5);
    void increase_capacity(int additional);
    int size() const;
    inline char** begin() { return strs; }
    inline char** end()   { return last_str; }
    // caller is responsible for freeing the mem
    char** to_str_array();
};

class Broker: public broker{
public:
    Broker(const std::string_view&n, int ccy_num, cash_balance* first_ccy_balance, char* yyyymmdd, strings* active_funds);
    ~Broker();

    inline CashBalance* head(default_member_tag) { return static_cast<CashBalance*>(first_cash_balance); }
    inline int size(default_member_tag) const { return num; } 
    Iterator<CashBalance> begin() { return Iterator( head(default_member_tag()) ); }
    Iterator<CashBalance> end() { return Iterator( head(default_member_tag()) + num ); }

    struct active_fund_tag{};
    inline char** head(active_fund_tag) { return static_cast<Strings*>(active_fund_ids)->begin(); }
    inline int size(active_fund_tag) { return static_cast<Strings*>(active_fund_ids)->size(); }
    Iterator<char*> fund_begin() { return Iterator( head(active_fund_tag()) ); }
    Iterator<char*> fund_end() { return Iterator(static_cast<Strings*>(active_fund_ids)->end()); }
};

class AllBrokers: public all_brokers{
public:
    AllBrokers(int n, broker* broker);
    ~AllBrokers();
    inline Broker* head(default_member_tag) { return static_cast<Broker*>(first_broker); }
    inline int size(default_member_tag) const { return num; } 
    Iterator<Broker> begin() { return Iterator( head(default_member_tag()) ); }
    Iterator<Broker> end() { return Iterator( head(default_member_tag()) + num ); }
};

class Fund: public fund{
public:
    Fund(const std::string_view& broker,  const std::string_view& name,  int amount, double capital, double market_value, double price, double profit, double roi, timestamp date);
    ~Fund();
};

class FundPortfolio: public fund_portfolio{
public:
    FundPortfolio(int n, fund* fund);
    ~FundPortfolio();
    inline Fund* head(default_member_tag) { return static_cast<Fund*>(first_fund); }
    inline int size(default_member_tag) const { return num; }

    Iterator<Fund> begin() { return Iterator( head(default_member_tag()) ); }
    Iterator<Fund> end() { return Iterator( head(default_member_tag()) + num ); }

    PtrIterator<Fund*> ptr_begin() { return PtrIterator( head(default_member_tag()) ); }
    PtrIterator<Fund*> ptr_end()   { return PtrIterator( head(default_member_tag()) + num ); }
};
class Quote: public quote{
public:
    Quote(const std::string& s, timestamp t, double r);
    ~Quote();
};

class Quotes: public quotes{
public:
    Quotes(int n, quote* head){
        num = n;
        first = head;
    }
    ~Quotes(){
        free_placement_allocated_structs<Quotes, Quote>(this);
        delete []first;
    }
    inline Quote* head(default_member_tag) { return static_cast<Quote*>(first); }
    inline int size(default_member_tag) const { return num; }
    Iterator<Quote> begin() { return Iterator(head(default_member_tag())); }
    Iterator<Quote> end()   { return Iterator(head(default_member_tag()) + num); }
};

class QuoteBySymbol{
public:
    explicit QuoteBySymbol(std::function<void(quotes*)> const& onLoaded):notify(onLoaded){}
    std::function<void(quotes*)> notify;
    std::unordered_map<std::string, const Quote*> mapping;
    inline void add(const std::string& sym, const Quote* q) {  mapping[sym] = q; }
};
void get_all_quotes(QuoteBySymbol& quotes_by_symbol, OnProgress OnProgress, void* progress_ctx);

class OverviewItem : public overview_item{
public:
    OverviewItem(const std::string& name, const std::string& ccy, double value, double value_in_main_ccy, double profit, double profit_in_main_ccy);
    ~OverviewItem();
};

class OverviewItemList : public overview_item_list{
public:
    OverviewItemList(int num, overview_item* head);
    ~OverviewItemList();
    inline OverviewItem * head(default_member_tag) { return static_cast<OverviewItem*>(first); }
    inline int size(default_member_tag) const { return num; }

    Iterator<OverviewItem> begin() { return Iterator( head(default_member_tag()) ); }
    Iterator<OverviewItem> end() { return Iterator( head(default_member_tag()) + num ); }
};

class OverviewItemContainer: public overview_item_container{
public:
    OverviewItemContainer(const std::string& name, const std::string& item_name,double value_sum_in_main_ccy, double profit_sum_in_main_ccy, int num, overview_item* head);
    ~OverviewItemContainer();
    inline OverviewItem * head(default_member_tag) { return static_cast<OverviewItem*>(items); }
    inline int size(default_member_tag) const { return num; }

    Iterator<OverviewItem> begin() { return Iterator( head(default_member_tag()) ); }
    Iterator<OverviewItem> end() { return Iterator( head(default_member_tag()) + num ); }
};

class OverviewItemContainerContainer: public overview_item_container_container{
public:
    OverviewItemContainerContainer(const std::string& name, const std::string& item_name, double value_sum_in_main_ccy, double profit_sum_in_main_ccy,int num, overview_item_container* head);
    ~OverviewItemContainerContainer();
    inline OverviewItemContainer* head(default_member_tag) { return static_cast<OverviewItemContainer*>(containers); }
    inline int size(default_member_tag) const { return num; }

    Iterator<OverviewItemContainer> begin() { return Iterator( head(default_member_tag()) ); }
    Iterator<OverviewItemContainer> end() { return Iterator( head(default_member_tag()) + num ); }
};

class Overview: public overview{
public:
    Overview(const std::string& item_name, double value_sum_in_main_ccy, double profit_sum_in_main_ccy,int num, overview_item_container_container* head);
    ~Overview();
    inline OverviewItemContainerContainer * head(default_member_tag) { return static_cast<OverviewItemContainerContainer*>(first); }
    inline int size(default_member_tag) const { return num; }

    Iterator<OverviewItemContainerContainer> begin() { return Iterator( head(default_member_tag()) ); }
    Iterator<OverviewItemContainerContainer> end() { return Iterator( head(default_member_tag()) + num ); }
};


// the ground rule is that none of the extended C++ classes should add member variables
// so the size of the class remain the same as the original struct
static_assert(sizeof(AllBrokers) == sizeof(all_brokers));
static_assert(sizeof(Broker) == sizeof(broker));
static_assert(sizeof(CashBalance)  == sizeof(cash_balance));
static_assert(sizeof(Fund)  == sizeof(fund));
static_assert(sizeof(FundPortfolio)  == sizeof(fund_portfolio));
static_assert(sizeof(Strings)  == sizeof(strings));
static_assert(sizeof(Quote)  == sizeof(quote));
static_assert(sizeof(Quotes)  == sizeof(quotes));
static_assert(sizeof(OverviewItem)  == sizeof(overview_item));
static_assert(sizeof(OverviewItemList)  == sizeof(overview_item_list));
static_assert(sizeof(OverviewItemContainer)  == sizeof(overview_item_container));
static_assert(sizeof(OverviewItemContainerContainer)  == sizeof(overview_item_container_container));
static_assert(sizeof(Overview)  == sizeof(overview));

#endif
