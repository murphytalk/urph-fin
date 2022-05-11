#ifndef URPH_FIN_STORAGE_HXX_
#define URPH_FIN_STORAGE_HXX_

#include <memory>
#include <stdexcept>
#include <sstream> 
#include <string>
#include <cstring>
#include <functional>
#include <vector>
#include <map>
 #include <execution>

#ifdef GTEST_INCLUDE_GTEST_GTEST_H_
#include <iostream>
#define LOG(x) std::cout
#else
// 3rd party
#include "aixlog.hpp"
#endif

#include "../core/urph-fin-core.hxx"
#include "../core/stock.hxx"

class BrokerBuilder{
    cash_balance* balances;
public:
    int ccy_num;
    cash_balance* head;
    Strings* funds;
    BrokerBuilder(int n, int active_fund_num) { 
        ccy_num = n; 
        funds = new Strings(active_fund_num);
        balances = new cash_balance[ccy_num];
        head = balances;
    }
    void add_cash_balance(const std::string& currency, double balance){
        LOG(DEBUG) << "  " << currency << " " << balance << " \n";
        new (balances++) CashBalance(currency, balance);
    }
    void add_active_fund(const std::string& active_fund_id){
        funds->add(active_fund_id);
    }
};

template<typename DAO, typename BrokerType>
Broker* create_broker(
    DAO* dao,
    const BrokerType& broker, 
    std::function<Broker*(const std::string&/*name*/, int/*ccy_num*/, cash_balance* /*first_ccy_balance*/, strings* /*active_fund_ids*/)> create_func)
{
    const BrokerBuilder& f = dao->get_broker_cash_balance_and_active_funds(broker);
    return create_func(dao->get_broker_name(broker), f.ccy_num, f.head, f.funds);
}

template<typename DAO, typename BrokerType>
class AllBrokerBuilder{
    broker* all_brokers;
public:
    int broker_num;
    broker* head;
    AllBrokerBuilder(int n){
        broker_num = n;
        all_brokers = new broker [n];
        head = all_brokers;
    }
    Broker* add_broker(DAO* dao, const BrokerType& b){
        return create_broker(dao, b, [&](const std::string&n, int ccy_num, cash_balance* first_ccy_balance, strings* active_funds){
            return new (all_brokers++) Broker(n, ccy_num, first_ccy_balance, active_funds);
        });
    }
};

class StringsBuilder{
public:
    Strings *strings;
    StringsBuilder(int n){
        strings = new Strings(n);
    }
    void add(const std::string& str){
        strings->add(str);
    }        
};

class FundsBuilder{
public:
    typedef PlacementNew<fund> FundAlloc;
    FundAlloc* fund_alloc;

    static FundsBuilder* create(int funds_num,std::function<void(FundAlloc*)> called_when_succeed){
        return new FundsBuilder(funds_num, called_when_succeed);
    }

   ~FundsBuilder(){
        delete fund_alloc;
    }
    Fund* add_fund(const std::string& broker,  const std::string& name,  const std::string& id, int amount, double capital, double market_value, double price, double profit, double roi, timestamp date){
        return new (fund_alloc->current++) Fund(
                                            broker, name, id,
                                            amount,
                                            capital,
                                            market_value,
                                            price,
                                            profit,
                                            roi,
                                            date
                                        );
    }
    inline void succeed() { 
        onSuccess(fund_alloc); 
        delete this;
    }
    inline void failed(){
        delete this;
    }
private:    
    std::function<void(FundAlloc*)> onSuccess;
    // ensure this cannot be allocated in stack
    FundsBuilder(int funds_num,std::function<void(FundAlloc*)> called_when_succeed){
        fund_alloc = new PlacementNew<fund>(funds_num);
        onSuccess = called_when_succeed;
    }
 };

class StockPortfolioBuilder{
public:
    ~StockPortfolioBuilder(){
        delete stock_alloc;
        for(const auto& i: tx){
            delete i.second;
        }
    }
    typedef PlacementNew<stock> StockAlloc;
    typedef PlacementNew<stock_tx> TxAlloc;
    typedef std::map<std::string, TxAlloc*> TxAllocPointerBySymbol;
    typedef std::function<void(StockAlloc*, const TxAllocPointerBySymbol&)> OnSuccess;

    static StockPortfolioBuilder* create(OnSuccess callback){
        return new StockPortfolioBuilder(callback);
    }

    StockAlloc* stock_alloc; 
    Stock* add_stock(const std::string& symbol, const std::string& ccy){
        return new (stock_alloc->current++) Stock(symbol, ccy);
    }
    void prepare_stock_alloc(int n) {
        stock_alloc = new StockAlloc(n);
    }
    void prepare_tx_alloc(const std::string& symbol, int num){
        tx[symbol] = new TxAlloc(num);
    }
    void addTx(const std::string& broker,const std::string& symbol, const std::string& type, const double price, const double shares, const double fee, const timestamp date){
        const auto s = tx.find(symbol);
        if(s != tx.end()){
            auto& tx_alloc = s->second;
            new (tx_alloc->current++) StockTx(broker, shares, price, fee, type);
            check_completion();
        }
        else throw std::runtime_error("Cannot find tx for stock " + symbol);
    }
    void check_completion(){
        if(std::find_if(std::execution::par,tx.begin(), tx.end(),[](const auto& alloc){ return !alloc.second->has_enough_counter(); }) == tx.end()){
            // no more pending
            onSuccess(stock_alloc, tx);
            delete this;
        }
    }
    void incr_counter(const std::string& symbol){
        const auto s = tx.find(symbol);
        if(s != tx.end()){
            s->second->inc_counter();
        }
    }
    inline void failed(){
        delete this;
    }
private:
    OnSuccess onSuccess;
    StockPortfolioBuilder(OnSuccess on_success){ 
        stock_alloc = nullptr;
        onSuccess = on_success;
    }
    TxAllocPointerBySymbol tx;
};

class IStorage{
public:
    virtual ~IStorage(){}
    virtual Broker* get_broker(const char* name) = 0;
    virtual AllBrokers* get_brokers() = 0;
    virtual strings* get_all_broker_names(size_t& size) = 0;
    virtual void get_funds(int funds_num, const char **fund_ids_head, OnFunds onFunds, void* onFundsCallerProvidedParam) = 0;
    virtual void get_stock_portfolio(const char* broker, const char* symbol, OnAllStockTx onAllStockTx, void* caller_provided_param) = 0;
    virtual strings* get_known_stocks(const char* broker) = 0;
};

template<typename DAO>
class Storage: public IStorage{
    std::unique_ptr<DAO> dao;
public:
    Storage(OnInitDone onInitDone){
        dao = std::make_unique<DAO>(onInitDone);
        LOG(DEBUG) << "Storage instance created\n";
    };
    virtual ~Storage(){
        LOG(DEBUG) << "Storage instance freed\n";
    }

    Broker* get_broker(const char* name)
    {
        return create_broker(dao.get(), dao->get_broker_by_name(name), [](const std::string&n, int ccy_num, cash_balance* first_ccy_balance, strings* active_funds){
            return new Broker(n, ccy_num, first_ccy_balance, active_funds);
        });
    }

    AllBrokers* get_brokers(){
        auto* p = dao->get_brokers();
        AllBrokers * b = new AllBrokers(p->broker_num, p->head);
        delete p;    
        return b;
    }

    strings* get_all_broker_names(size_t& size) { 
        std::unique_ptr<StringsBuilder> builder(dao->get_all_broker_names(size));
        return builder->strings;
    }

    void get_funds(int funds_num, const char **fund_ids_head, OnFunds onFunds, void* onFundsCallerProvidedParam){
        // self delete upon finish
        auto *p = FundsBuilder::create(funds_num,[onFunds, onFundsCallerProvidedParam](FundsBuilder::FundAlloc* fund_alloc){
            std::sort(fund_alloc->head, fund_alloc->head + fund_alloc->allocated_num(),[](fund& f1, fund& f2){ 
                auto byBroker = strcmp(f1.broker,f2.broker);
                auto v = byBroker == 0 ? strcmp(f1.name, f2.name) : byBroker;
                return v < 0; 
            });
            onFunds(new FundPortfolio(fund_alloc->allocated_num(), fund_alloc->head), onFundsCallerProvidedParam);
        });
        dao->get_funds(p, funds_num, fund_ids_head);
    }

    void get_stock_portfolio(const char* broker, const char* symbol, OnAllStockTx onAllStockTx, void* caller_provided_param){
        // self delete upon finish
        auto *builder = 
            StockPortfolioBuilder::create([onAllStockTx, caller_provided_param](StockPortfolioBuilder::StockAlloc* stock_alloc, const StockPortfolioBuilder::TxAllocPointerBySymbol& tx){
                stock_with_tx* head = new stock_with_tx[stock_alloc->allocated_num()];
                stock_with_tx* current = head;
                for(auto* b = stock_alloc->head; b != stock_alloc->end(); ++b){
                    auto tx_iter = tx.find(b->symbol);      
                    if(tx_iter == tx.end()){
                        throw std::runtime_error(std::string("Cannot find tx for stock ") + b->symbol);
                    }
                    else{
                        new (current++) StockWithTx(b, new StockTxList(tx_iter->second->allocated_num(), tx_iter->second->head));
                    }    
                }
                onAllStockTx(new StockPortfolio(stock_alloc->allocated_num(), head), caller_provided_param);
            });
        dao->get_stock_portfolio(builder, broker, symbol);
    }
    strings* get_known_stocks(const char* broker) { 
        const auto& builder = dao->get_known_stocks(broker);
        return builder.strings; 
    }
};

IStorage * create_firestore_instance(OnInitDone onInitDone);

#endif
