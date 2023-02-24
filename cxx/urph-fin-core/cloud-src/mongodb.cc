#include <vector>
#include <functional>
#include <string>
#include <algorithm>
#include <memory>

#include "core/stock.hxx"
#include "storage/storage.hxx"
#include "core/core_internal.hxx"

#include <cstdint>
#include <iostream>
#include <vector>

#include <cstdint>
#include <iostream>
#include <vector>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/instance.hpp>
#include <bsoncxx/builder/stream/helpers.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/stream/array.hpp>


using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::open_document;

namespace{
    const char DB_NAME[] = "urph-fin";
    const char BROKER [] = "broker";
    const char INSTRUMENT [] = "instrument";

    //todo: run this when MongDbDao is created ?
}

#include "../generated-code/mongodb.cc"

class MongoDbDao
{
public:
    MongoDbDao(OnDone onInitDone){
    }
    ~MongoDbDao(){
    }
    typedef int BrokerType;
    void get_broker_by_name(const char *, std::function<void(const BrokerType&)> onBrokerData) {}
    std::string get_broker_name(const BrokerType&) { return std::string("");}
    void get_broker_cash_balance_and_active_funds(const BrokerType &broker_query_result, std::function<void(const BrokerBuilder&)> onBrokerBuilder){}
    void get_brokers(std::function<void(AllBrokerBuilder<MongoDbDao, BrokerType>*)>){}
    void get_funds(FundsBuilder *, int, char* ,const char **) {}
    void get_known_stocks(OnStrings, void *ctx) {}
    void get_non_fund_symbols(std::function<void(Strings *)> onResult) {}
    void get_latest_quotes(LatestQuotesBuilder *builder, int num, const char **symbols_head) {}
    void get_latest_quotes(LatestQuotesBuilder *builder) {}
    void add_tx(const char *broker, const char *symbol, double shares, double price, double fee, const char *side, timestamp date,
                OnDone onDone, void *caller_provided_param) {}
    void get_stock_portfolio(StockPortfolioBuilder *builder, const char *broker, const char *symbol)
    {
    }
};


IDataStorage *create_cloud_instance(OnDone onInitDone) { 
    mongocxx::instance instance{};
    return new Storage<MongoDbDao>(onInitDone); 
}