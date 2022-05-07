#ifndef URPH_FIN_STORAGE_HXX_
#define URPH_FIN_STORAGE_HXX_

#include <memory>
#include <stdexcept>
#include <sstream> 
#include <string>
#include <functional>
#include <vector>

// 3rd party
#include "aixlog.hpp"

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
        LOG(DEBUG) << "  " << currency << " " << balance << "" "\n";
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

class AllBrokerNamesBuilder{
public:
    char **all_broker_names;
    AllBrokerNamesBuilder(int n){
        all_broker_names = new char* [n];
    }
    void add_name(int i, const std::string& name){
        all_broker_names[i] = copy_str(name);
    }
};

class IStorage{
public:
    virtual ~IStorage(){}
    virtual Broker* get_broker(const char* name) = 0;
    virtual AllBrokers* get_brokers() = 0;
    virtual char** get_all_broker_names(size_t& size) = 0;
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
    char** get_all_broker_names(size_t& size) { 
        std::unique_ptr<AllBrokerNamesBuilder> builder(dao->get_all_broker_names(size));
        return builder->all_broker_names;
    }
    void get_funds(int funds_num, const char **fund_ids_head, OnFunds onFunds, void* onFundsCallerProvidedParam){

    }
    void get_stock_portfolio(const char* broker, const char* symbol, OnAllStockTx onAllStockTx, void* caller_provided_param){

    }
    strings* get_known_stocks(const char* broker) { return dao->get_known_stocks(broker); }
};

IStorage * create_firestore_instance(OnInitDone onInitDone);

#endif
