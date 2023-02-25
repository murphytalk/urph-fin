#include <vector>
#include <functional>
#include <string>
#include <algorithm>
#include <memory>

#include "../src/utils.hxx"
#include "core/stock.hxx"
#include "storage/storage.hxx"
#include "core/core_internal.hxx"

#include <cstdint>
#include <iostream>
#include <vector>
#include <memory>

#include <cstdint>
#include <iostream>
#include <vector>
#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/exception/exception.hpp>
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

    const char tag[] = "mongodb";
}

#include "../generated-code/mongodb.cc"

class MongoDbDao
{
    std::unique_ptr<mongocxx::client> client;
    #define DB client->database(DB_NAME)
    #define BROKER_COLLECTION DB[BROKER]
    #define INSTRUMENT_COLLECTION DB[INSTRUMENT]
public:
    MongoDbDao(OnDone onInitDone){
        LINFO(tag, "connecting to " << mongodb_conn_str);
        try{
            client = std::make_unique<mongocxx::client>(mongocxx::uri(mongodb_conn_str));
            LINFO(tag, "client status " << (bool)*client);
        }
        catch(mongocxx::exception& ex){
            std::cout << ex.what() << std::endl;
        }
        onInitDone(nullptr);
    }
    typedef bsoncxx::document::view BrokerType;
    void get_broker_by_name(const char *broker, std::function<void(const BrokerType&)> onBrokerData) {
        const auto b = BROKER_COLLECTION.find_one( document{} << "name" << broker << finalize );
        if(b){
            onBrokerData(*b);
        }
    }
    std::string_view get_broker_name(const BrokerType& broker) { 
        return broker["name"].get_string().value;
    }
    void get_broker_cash_balance_and_active_funds(const BrokerType &broker_query_result, std::function<void(const BrokerBuilder&)> onBrokerBuilder){
        const auto &name = get_broker_name(broker_query_result);
        LDEBUG(tag, "Broker " << name);

        const auto& cashObj = broker_query_result["cash"].get_document().view();

        auto addCash = [&cashObj](BrokerBuilder &b)
        {
            for (const auto &c : cashObj)
            {
                double balance = c.get_value().get_double();
                LDEBUG(tag, "  " << c.key() << " " << balance );
                b.add_cash_balance(c.key(), balance);
            }
        };

        auto onlyAddCash = [&cashObj, &addCash]()
        {
            auto b = BrokerBuilder(cashObj.length(), 0);
            addCash(b);
        };

        auto funds_update_date_element = broker_query_result["funds_update_date"];
        if(funds_update_date_element){
            const auto& funds_update_date = funds_update_date_element.get_value().get_string().value;
            LDEBUG(tag, " last funds update date: " << funds_update_date );
            
            const auto& funds = broker_query_result["active_funds"].get_document().view()[funds_update_date].get_array().value;

            auto b = BrokerBuilder(cashObj.length(), funds.length());
            b.set_fund_update_date(funds_update_date);
            addCash(b);

            if(funds.length() == 0){
                onlyAddCash();
            }
            else{
                for (const auto &f : funds)
                {
                    b.add_active_fund(f.get_string());
                }
            }
            onBrokerBuilder(b);
         }
    }
    void get_brokers(std::function<void(AllBrokerBuilder<MongoDbDao, BrokerType>*)> onAllBrokersBuilder){
        auto *all = new AllBrokerBuilder<MongoDbDao, BrokerType>(5 /*init value, will get increased automatically*/);
        auto cursor = BROKER_COLLECTION.find({});
        for(const auto broker: cursor){
            LDEBUG(tag, "adding broker");
            all->add_broker(this, broker);
        }
        onAllBrokersBuilder(all);
    }
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