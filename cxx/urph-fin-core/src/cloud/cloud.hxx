#ifndef URPH_FIN_CLOUD_HXX_
#define URPH_FIN_CLOUD_HXX_

#include "../core/urph-fin-core.hxx"
#include "../core/stock.hxx"

class Cloud{
public:
    Cloud(){};
    virtual broker* get_broker(const char* name) = 0;
    virtual AllBrokers* get_brokers() = 0;
    virtual char** get_all_broker_names(size_t& size) = 0;
    virtual void get_funds(int funds_num, const char **fund_ids_head, OnFunds onFunds, void* onFundsCallerProvidedParam) = 0;
    virtual void get_stock_portfolio(const char* broker, const char* symbol, OnAllStockTx onAllStockTx, void* caller_provided_param) = 0;
    virtual strings* get_known_stocks(const char* broker) = 0;
};

Cloud* create_firestore_instance(OnInitDone onInitDone);

#endif