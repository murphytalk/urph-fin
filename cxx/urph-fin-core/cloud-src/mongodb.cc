#include <type_traits>
#include <vector>
#include <functional>
#include <string>
#include <algorithm>
#include <memory>

#include "../src/utils.hxx"
#include "bsoncxx/document/view.hpp"
#include "core/stock.hxx"
#include "mongocxx/cursor.hpp"
#include "storage/storage.hxx"
#include "core/core_internal.hxx"

#include <cstdint>
#include <algorithm>
#include <vector>
#include <memory>
#include <thread>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>

#include <cstdint>
#include <iostream>
#include <iomanip>
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

#include <chrono>
#include <iomanip>
#include <ctime>

using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::document;
using bsoncxx::builder::stream::finalize;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::array;
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::open_document;

namespace{
    const char DB_NAME[] = "urph-fin";
    const char BROKER [] = "broker";
    const char INSTRUMENT [] = "instrument";

    const char tag[] = "mongodb";
    double safe_get_double(const bsoncxx::document::element& e){
        const auto& v = e.get_value();
        return v.type() == bsoncxx::type::k_int32 ? (double)v.get_int32() : v.get_double();
    }
    timestamp safe_get_timestamp(const bsoncxx::document::element& e){
        const auto& v = e.get_value();
        switch (v.type())
        {
        case bsoncxx::type::k_double:
            return (timestamp)v.get_double();
        case bsoncxx::type::k_int32:
            return v.get_int32();
        case bsoncxx::type::k_int64:
            return v.get_int64();
        default:
            return 0;
        }
    }

    std::string formatUnixEpochToYYYYMMDD(const char* prefix , int64_t epoch) {
        // Convert Unix epoch to time_t
        std::time_t time = static_cast<time_t>(epoch);

        // Convert time_t to tm struct
        std::tm* tm_ptr = std::gmtime(&time);

        // Use stringstream for formatting
        std::stringstream ss;
        ss << prefix << std::put_time(tm_ptr, "%Y%m%d");

        return ss.str();
    }

}

#include "../generated-code/mongodb.cc"

class MongoDbDao
{
    #define DB client->database(DB_NAME)
    #define BROKER_COLLECTION DB[BROKER]
    #define INSTRUMENT_COLLECTION DB[INSTRUMENT]


    std::mutex mongo_conn_mutex;
    std::unique_ptr<mongocxx::client> client;
public:
    MongoDbDao(OnDone onInitDone, void* caller_provided_param)
    {
        LINFO(tag, "mongodb worker thread started, connecting to " << mongodb_conn_str);
        client = std::make_unique<mongocxx::client>(mongocxx::uri(mongodb_conn_str));
        LINFO(tag, "client status " << (bool)*client);
        onInitDone(caller_provided_param);
    }

    typedef bsoncxx::document::view BrokerType;
    void get_broker_by_name(const char *broker, std::function<void(const BrokerType&)> onBrokerData) {
        // cast to void to deliberately ignore the [[nodiscard]] attribute
        (void)get_thread_pool()->submit([this, broker, onBrokerData=std::move(onBrokerData)](){
            std::lock_guard<std::mutex> lock(mongo_conn_mutex);
            const auto b = BROKER_COLLECTION.find_one( document{} << "name" << broker << finalize );
            if(b){
                onBrokerData(*b);
            }
        });
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
                double balance = safe_get_double(c);  // c.get_value().get_double();
                LDEBUG(tag, "  " << c.key() << " " << balance );
                b.add_cash_balance(c.key(), balance);
            }
        };

        auto onlyAddCash = [&cashObj, &addCash,&onBrokerBuilder]()
        {
            auto b = BrokerBuilder(cashObj.length(), 0);
            addCash(b);
            onBrokerBuilder(b);
        };

        auto funds_update_date_element = broker_query_result["funds_update_date"];
        if(funds_update_date_element){
            const auto& funds_update_date = funds_update_date_element.get_string();
            LDEBUG(tag, " last funds update date: " << funds_update_date.value );

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
         else onlyAddCash();
    }

    void get_brokers(std::function<void(AllBrokerBuilder<MongoDbDao, BrokerType>*)> onAllBrokersBuilder){
        (void)get_thread_pool()->submit([this,onAllBrokersBuilder=std::move(onAllBrokersBuilder)](){
            std::lock_guard<std::mutex> lock(mongo_conn_mutex);
            auto *all = new AllBrokerBuilder<MongoDbDao, BrokerType>(5 /*init value, will get increased automatically*/);
            auto cursor = BROKER_COLLECTION.find({});
            for(const auto broker: cursor){
                LDEBUG(tag, "adding broker");
                all->add_broker(this, broker);
            }
            onAllBrokersBuilder(all);
        });
    }
    void get_funds(FundsBuilder *builder, std::vector<FundsParam>&& params){
        (void)get_thread_pool()->submit([=](){
            std::lock_guard<std::mutex> lock(mongo_conn_mutex);
            for(const auto& param: params){
                const char* fund_name = param.name.c_str();
                const auto fund = INSTRUMENT_COLLECTION.find_one( document{} << "name" << fund_name << finalize );
                if(fund){
                    LDEBUG(tag, "found fund " << fund_name);
                    const auto& fund_doc = *fund;
                    const auto& tx = fund_doc["tx"].get_document().view()[param.update_date].get_document().view();
                    const std::string_view& broker = tx["broker"].get_string();
                    const int amt = tx["amount"].get_int32();
                    const double capital = safe_get_double(tx["capital"]);
                    const double market_value = safe_get_double(tx["market_value"]);
                    const double price = safe_get_double(tx["price"]);
                    const double profit = market_value - capital;
                    const double roi = profit / capital;
                    const timestamp date = tx["date"].get_int32();
                    LDEBUG(tag, "got tx of broker " << broker.data() << " on epoch=" << date << " fund=" << fund_name );
                    builder->add_fund(
                        broker, fund_name,
                        amt,
                        capital,
                        market_value,
                        price,
                        profit,
                        roi,
                        date
                    );
                    if(builder->alloc->has_enough_counter()) builder->succeed(); else{
                        LDEBUG(tag, "not enough counter, waiting for more");
                    }
                }
                else{
                    LERROR(tag, "Missing fund " << fund_name);
                }
            }
        });
    }

    void get_known_stocks(OnStrings onStrings, void *ctx) {
        auto projection = new bsoncxx::builder::stream::document();
        *projection << "_id" << 0 << "name" << 1 ;
        get_stock_and_etf<StringsBuilder>(new StringsBuilder(10),
            nullptr, nullptr,  projection,
            [](StringsBuilder* b, const bsoncxx::document::view& doc_view){ b->add(doc_view["name"].get_string()); },
            [=](StringsBuilder* b){
                onStrings(b->strings, ctx);
                delete b;
            }
        );
    }

    void get_latest_quotes(LatestQuotesBuilder *builder, int num, const char **symbols_head) {}
    void get_latest_quotes(LatestQuotesBuilder *builder) {}

    void add_tx(const char *broker, const char *symbol, double shares, double price, double fee, const char *side, timestamp date,
                OnDone onDone, void *caller_provided_param) {
        bsoncxx::builder::stream::document subdocument_builder{};
        subdocument_builder << "broker" << broker
                            << "instrument_id" << symbol
                            << "date" << date
                            << "fee" << fee
                            << "price" << price
                            << "shares" << shares
                            << "type" << side;

        // The update command
        bsoncxx::builder::stream::document update_builder{};
        update_builder << "$set" << bsoncxx::builder::stream::open_document 
                    << formatUnixEpochToYYYYMMDD("tx.",date) << subdocument_builder.view()
                    << bsoncxx::builder::stream::close_document;

        // The filter to find the document you want to update
        bsoncxx::builder::stream::document filter_builder{};
        filter_builder << "name" << symbol; 

        // Perform the update operation
        INSTRUMENT_COLLECTION.update_one(filter_builder.view(), update_builder.view());

    }

    void get_stock_portfolio(StockPortfolioBuilder *builder, const char *broker, const char *symbol)
    {
        builder->prepare_stock_alloc_dont_know_total_num(10);
        auto projection = new bsoncxx::builder::stream::document();
        *projection << "_id" << 0 ;
        get_stock_and_etf<StockPortfolioBuilder>(
            builder, symbol, broker, projection,
            [](StockPortfolioBuilder* b, const bsoncxx::document::view& doc_view){
                const std::string_view& my_symbol = doc_view["name"].get_string();
                const std::string_view& ccy = doc_view["ccy"].get_string();
                b->add_stock(my_symbol, ccy);
                const auto& tx = doc_view["tx"].get_document().view();
                auto tx_num = std::distance(tx.begin(), tx.end());
                b->prepare_tx_alloc(std::string(my_symbol), tx_num);
                for(auto& tx_obj: tx){
                    try{
                        const auto& v = tx_obj.get_document().view();
                        const double price = safe_get_double(v["price"]);
                        const timestamp date = safe_get_timestamp(v["date"]);
                        const std::string_view& type = v["type"].get_string();
                        const std::string_view& my_broker = v["broker"].get_string();

                        double shares = 0;
                        double fee = 0;
                        if(type != "SPLIT"){
                            shares = safe_get_double(v["shares"]);
                            fee = safe_get_double(v["fee"]);
                        }
                        b->addTx(my_broker, my_symbol, type, price, shares, fee, date);
                    }
                    catch(const std::exception& ex){
                        LERROR(log, "failed to get stock tx " << ex.what());
                    }
                }
            },
            [](StockPortfolioBuilder *b){ b->complete(); }
        );
     }
private:
    #define MV_STR(p) std::move(std::string(p == nullptr?"":p))
    template<typename T>
    void get_stock_and_etf(
            T* context,
            const char* symbol,
            const char* broker,
            bsoncxx::builder::stream::document* projection, //will be freed by this function
            std::function<void(T*, const bsoncxx::document::view&)> onInstrument,
            std::function<void(T* context)> onFinish)
    {
        LDEBUG(tag, "get stock&etf: broker=" << (broker == nullptr ? "null" : broker) << ",sym=" << (symbol == nullptr ? "null" : symbol));
        (void)get_thread_pool()->submit([this, context, symbol=MV_STR(symbol), broker=MV_STR(broker), projection,
                                         onInstrument=std::move(onInstrument), onFinish=std::move(onFinish)](){

            std::lock_guard<std::mutex> lock(mongo_conn_mutex);

            auto iterate_stock = [context,&onInstrument](mongocxx::cursor&& cursor){
              for (auto doc_view : cursor){
                //auto has = doc_view.find("type") != doc_view.end();
                //LINFO(tag, "has type = " << has);
                onInstrument(context, doc_view);
              }
            };
            document filter_builder{};
            filter_builder << "type" << open_document << "$in" << open_array << "Stock" << "ETF" << close_array << close_document;

            if(broker.size() == 0){
                LDEBUG(tag, "broker not specified");
                if ( symbol.size() !=0 ) {
                    filter_builder << "name" << symbol;
                    LDEBUG(tag, "sym = " << symbol);
                }

                mongocxx::options::find opts{};
                if(projection != nullptr){
                    opts.projection(projection->view());
                }
                iterate_stock(INSTRUMENT_COLLECTION.find(filter_builder.view(), opts));
            }
            else{
                /* basically this native query
                db.instrument.aggregate([
                  {
                    "$match": {
                      "name": "NFLX"
                    }
                  },
                  {
                    "$addFields": {
                      "tx": {
                        "$filter": {
                          "input": { "$objectToArray": "$tx" },
                          "as": "tx_doc",
                          "cond": { "$eq": [ "$$tx_doc.v.broker", "IB" ] }
                        }
                      }
                    }
                  },
                  {
                    "$addFields": {
                      "tx": { "$arrayToObject": "$tx" }
                    }
                  },
                  {
                    "$project": {
                      "_id": 0,
                    }
                  }
                ])
                */
                mongocxx::pipeline pipeline{};
                pipeline.match(filter_builder << "name" << symbol << finalize);

                pipeline.sort(document{} << "name" << 1 << finalize);

                auto filter = document{} << "tx" << open_document <<
                    "$filter" << open_document <<
                        "input" << open_document <<
                            "$objectToArray" << "$tx" << close_document <<
                        "as" << "tx_doc" <<
                        "cond" << open_document <<
                            "$eq" << open_array << "$$tx_doc.v.broker" << broker << close_array <<
                        close_document << close_document << close_document << finalize;
                pipeline.add_fields(filter.view());


                auto back2tx = document {} << "tx" << open_document <<
                    "$arrayToObject" << "$tx" << close_document <<  finalize;
                pipeline.add_fields(back2tx.view());

                if(projection != nullptr){
                    pipeline.project(projection->view());
                }

                iterate_stock(INSTRUMENT_COLLECTION.aggregate(pipeline));
            }


            onFinish(context);
            delete projection;
        });
    }
};


IDataStorage *create_cloud_instance(OnDone onInitDone, void* caller_provided_param) {
    mongocxx::instance instance{};
    return new Storage<MongoDbDao>(onInitDone, caller_provided_param);
}