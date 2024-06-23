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

#ifdef DEBUG_BUILD
#include <set>
#include <numeric>
#endif

#include <bsoncxx/json.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/logger.hpp>
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

    const char* ASSET_CLASS_RATIO_NAMES [] = {
        "stock", "bond", "metal", "cash"
    };

    double safe_get_double(const bsoncxx::document::element& e){
        const auto& v = e.get_value();
        return v.type() == bsoncxx::type::k_int32 ? (double)v.get_int32() : v.get_double();
    }
    int32_t safe_get_int32(const bsoncxx::document::element& e){
        const auto& v = e.get_value();
        return v.type() == bsoncxx::type::k_int32 ? v.get_int32() : (int32_t) v.get_double();
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

    class logger final : public mongocxx::logger {
        public:
            void operator()(mongocxx::log_level level,
                            bsoncxx::stdx::string_view domain,
                            bsoncxx::stdx::string_view message) noexcept override {
                //if (level >= mongocxx::log_level::k_trace)
                //    return;
                LDEBUG( '[' << mongocxx::to_string(level) << '@' << domain << "] " << message);
            }
    };    
}

#include "../generated-code/mongodb.cc"
extern "C" void mongoc_log_trace_enable();
class MongoDbDao
{
    #define DB client->database(DB_NAME)
    #define BROKER_COLLECTION DB[BROKER]
    #define INSTRUMENT_COLLECTION DB[INSTRUMENT]


    std::mutex mongo_conn_mutex;
    std::unique_ptr<mongocxx::instance> instance;
    std::unique_ptr<mongocxx::client> client;
public:
    MongoDbDao(OnDone onInitDone, void* caller_provided_param)
    {

        instance = std::make_unique<mongocxx::instance>(bsoncxx::stdx::make_unique<logger>());
        LINFO( "mongodb worker thread started, connecting to " << mongodb_conn_str);

        client = std::make_unique<mongocxx::client>(mongocxx::uri(mongodb_conn_str));
        LINFO( "client status " << (bool)*client);
        onInitDone(caller_provided_param);
        LDEBUG( "mongodb init done");
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
        LDEBUG( "Broker " << name);

        const auto& cashObj = broker_query_result["cash"].get_document().view();

        auto addCash = [&cashObj](BrokerBuilder &b)
        {
            for (const auto &c : cashObj)
            {
                double balance = safe_get_double(c);  // c.get_value().get_double();
                LDEBUG( "  " << c.key() << " " << balance );
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
            LDEBUG( " last funds update date: " << funds_update_date.value );

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
                LDEBUG( "adding broker");
                all->add_broker(this, broker);
            }
            onAllBrokersBuilder(all);
        });
    }
    void get_funds(FundsBuilder *builder, std::vector<FundsParam>&& params){
            LDEBUG( "getting fund");
#ifdef DEBUG_BUILD
            pending_funds.clear();
            for(const auto& param: params){
                pending_funds.insert(param.name);
            }
#endif
            for(const auto& param: params){
                const char* fund_name = param.name.c_str();
                const char* broker_name = param.broker.c_str();
                const char* tx_date = param.update_date.c_str();
                LINFO( "finding fund " << fund_name <<"@"<<tx_date);

                get_instrument_tx<FundsBuilder>(
                    builder,true,fund_name,broker_name,tx_date, nullptr,
                    [](FundsBuilder * b, const std::string_view& sym, const std::string_view&,asset_class_ratio& ratio, uint16_t){},
                    [this](FundsBuilder * builder,const std::string_view& sym, asset_class_ratio& assert_class_ratios,const bsoncxx::document::view& tx){
                        const std::string_view& broker = tx["broker"].get_string();
                        const int amt = safe_get_int32(tx["amount"]);
                        const double capital = safe_get_double(tx["capital"]);
                        const double market_value = safe_get_double(tx["market_value"]);
                        const double price = safe_get_double(tx["price"]);
                        const double profit = market_value - capital;
                        const double roi = profit / capital;
                        const timestamp date = safe_get_int32(tx["date"]);
                        LDEBUG( "got tx of broker " << broker.data() << " on epoch=" << date << " fund=" << sym);
                        builder->add_fund(
                            broker, sym,
                            amt,
                            capital,
                            market_value,
                            price,
                            profit,
                            roi,
                            std::move(assert_class_ratios),
                            date
                        );
                    },
                    [
#ifdef DEBUG_BUILD
                        this
#endif
                    ](FundsBuilder *builder, auto& sym){ 
                        if(builder->alloc->has_enough_counter()) 
                            builder->succeed(); 
                        else{
#ifdef DEBUG_BUILD
                            LDEBUG("Removing "<< sym);
                            pending_funds.erase(sym);
                            LDEBUG("Pending funds num = " << pending_funds.size() << ":" << 
                                std::accumulate(
                                    pending_funds.begin(),pending_funds.end(),*pending_funds.begin(),
                                     [](const std::string &a, const std::string &b) {
                                            return a + ", " + b;
                                        }
                                )
                            );
#endif
                            LDEBUG( "not enough counter, waiting for more");
                        }
                    }
                );
            }
            LINFO( "leaving get_funds");
    }

    void get_known_stocks(OnStrings onStrings, void *ctx) {
        auto projection = new bsoncxx::builder::stream::document();
        //TODO: cursor iteration would hang
        //*projection << "_id" << 0 << "name" << 1 ;
        *projection << "_id" << 0 ;
        get_instrument_tx<StringsBuilder>(
            new StringsBuilder(10),
            false,
            nullptr, nullptr, nullptr, projection,
            [](StringsBuilder* b, const std::string_view& sym, const std::string_view&,asset_class_ratio&,uint16_t){ 
                b->add(sym); 
            },
            [](StringsBuilder* b, const std::string_view&, asset_class_ratio&,const bsoncxx::document::view&){},
            [=](StringsBuilder* b,auto&){
                onStrings(b->strings, ctx);
                delete b;
            },
            true
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
        onDone(caller_provided_param);
    }

    void update_cash(const char* broker, const char* ccy, double balance,OnDone onDone,void* caller_provided_param){
        // The update command
        bsoncxx::builder::stream::document update_builder{};
        std::string upd = "cash.";
        upd += ccy;
        update_builder << "$set" << bsoncxx::builder::stream::open_document 
            << upd << balance << bsoncxx::builder::stream::close_document;
         // The filter to find the document you want to update
        bsoncxx::builder::stream::document filter_builder{};
        filter_builder << "name" << broker; 

        BROKER_COLLECTION.update_one(filter_builder.view(), update_builder.view());
        onDone(caller_provided_param);
    }

    void get_stock_portfolio(StockPortfolioBuilder *builder, const char *broker, const char *symbol)
    {
        builder->prepare_stock_alloc_dont_know_total_num(10);
        auto projection = new bsoncxx::builder::stream::document();
        *projection << "_id" << 0 ;
        get_instrument_tx<StockPortfolioBuilder>(
            builder, false, symbol, broker, nullptr, projection,
            [](StockPortfolioBuilder* b, const std::string_view& sym, const std::string_view& ccy,asset_class_ratio& class_ratio, uint16_t max_tx_num){
                b->add_stock(sym, ccy, class_ratio);
                b->prepare_tx_alloc(std::string(sym), max_tx_num);
            },
            [this](StockPortfolioBuilder* b,const std::string_view& sym, asset_class_ratio&,const bsoncxx::document::view& tx){
                add_tx(b, sym, tx);                
            },
            [](StockPortfolioBuilder *b,auto&){ b->complete(); }
        );
     }
private:
#ifdef DEBUG_BUILD
        std::set<std::string> pending_funds;
#endif
    void add_tx(StockPortfolioBuilder* b,const std::string_view& my_symbol, const bsoncxx::v_noabi::document::view& v){
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

    #define MV_STR(p) std::move(std::string(p == nullptr?"":p))

    template<typename T>
    void get_instrument_tx(
            T* context,
            bool is_fund,
            const char* symbol,
            const char* broker,  
            const char* tx_date, // nullptr => all tx
            bsoncxx::builder::stream::document* projection, //will be freed by this function
            std::function<void(T* ctx, const std::string_view& sym, const std::string_view& ccy, asset_class_ratio& class_ratio, uint32_t max_tx_num)>&& onInstrument,
            std::function<void(T* ctx, const std::string_view& sym, asset_class_ratio& class_ratio, const bsoncxx::document::view& tx)>&& onTx,
            std::function<void(T* ctx, const std::string& sym)>&& onFinish, 
            bool ignoreTx=false)
    {
        LDEBUG( "get tx: broker=" << (broker == nullptr ? "null" : broker) << ",sym=" << (symbol == nullptr ? "null" : symbol));
        (void)get_thread_pool()->submit([this, context, symbol=MV_STR(symbol), broker=MV_STR(broker), tx_date=MV_STR(tx_date),projection,
                                         is_fund,ignoreTx,
                                         onInstrument=std::move(onInstrument), onTx=std::move(onTx),onFinish=std::move(onFinish)](){

            std::lock_guard<std::mutex> lock(mongo_conn_mutex);

            document filter_builder {};
            if(is_fund)
                filter_builder << "type" << open_document << "$in" << open_array << "Funds" << close_array << close_document;
            else
                filter_builder << "type" << open_document << "$in" << open_array << "Stock" << "ETF" << close_array << close_document;

            if ( symbol.size() !=0 ) {
                filter_builder << "name" << symbol;
                LDEBUG( "sym = " << symbol);
            }

            mongocxx::options::find opts{};
            if(projection != nullptr){
                LDEBUG( "has projection");
                opts.projection(projection->view());
            }
            else{
                LDEBUG( "no projection");
            }

            auto expected_broker = [&broker](const bsoncxx::document::view& tx){
                return broker.size() == 0 ? true : broker == tx["broker"].get_string().value;
            };

            asset_class_ratio class_ratio {0,0,0,0};

            auto instrument = [context, &class_ratio, &onInstrument](const bsoncxx::document::view& doc_view, uint32_t tx_num){
                const std::string_view& my_symbol = doc_view["name"].get_string();
                const std::string_view& ccy = doc_view["ccy"].get_string();
                LDEBUG( "instrument name="<<my_symbol<<",ccy="<<ccy<<",tx num="<<tx_num);
                onInstrument(context, my_symbol, ccy, class_ratio, tx_num);
                return my_symbol;
            };

            auto process_tx_obj = [&expected_broker, context, &class_ratio, &onTx](const std::string_view& my_symbol,const bsoncxx::v_noabi::document::element& tx_obj){
                try{
                    if(tx_obj.type() == bsoncxx::type::k_array){
                        auto array = tx_obj.get_array().value;
                        for(auto&& o: array){
                            const auto& v = o.get_document().view();
                            if(expected_broker(v)) onTx(context, my_symbol, class_ratio, v);
                        }
                    }
                    else{ 
                        const auto& v = tx_obj.get_document().view();
                        if(expected_broker(v)) onTx(context, my_symbol, class_ratio,v);
                    }
                }
                catch(const std::exception& ex){
                    LERROR( "failed to get stock tx " << ex.what());
                }
            };

            auto get_tx_docs = [](const bsoncxx::document::view& doc_view,uint32_t& tx_num){
                auto tx_iter = doc_view.find("tx");
                if(tx_iter != doc_view.end()){
                    const auto& tx = tx_iter->get_document().view();
                    tx_num = std::distance(tx.begin(), tx.end());
                }
                return tx_iter;
            };

            auto get_ratio = [&class_ratio](const  bsoncxx::document::view&doc){
                const auto& asset_class = doc["asset_class"].get_document().view();
                for(int i = 0 ; i < sizeof(ASSET_CLASS_RATIO_NAMES)/sizeof(const char*); ++i){
                    auto f  = asset_class.find(ASSET_CLASS_RATIO_NAMES[i]);
                    class_ratio.set(i, f == asset_class.end() ? 0.0 : safe_get_double(*f));
                }
            };

            if(tx_date.size() > 0){
                LERROR( "looking for broker="<<broker<<",sym="<<symbol<<",tx date="<<tx_date);
                const auto doc = INSTRUMENT_COLLECTION.find_one(filter_builder.view(), opts);
                if(doc){
                    const auto& doc_view = doc->view();
                    get_ratio(doc_view);
                    uint32_t tx_num = 0;
                    get_tx_docs(doc_view, tx_num);
                    const auto& my_symbol = instrument(doc_view, tx_num);
                    LERROR( "get tx obj for broker="<<broker<<",sym="<<my_symbol<<",tx date="<<tx_date);
                    process_tx_obj(my_symbol, doc_view["tx"].get_document().view()[tx_date]);
                }
                else{
                    LERROR( "Missing sym=" << symbol << ",broker=" << broker << ",date=" << tx_date );
                }
            }
            else{
                auto cursor = INSTRUMENT_COLLECTION.find(filter_builder.view(), opts);
                LDEBUG( "about to iterate through cursor");
                for (auto&& doc_view : cursor){
                    get_ratio(doc_view);
                    uint32_t tx_num = 0;
                    auto tx_iter = get_tx_docs(doc_view, tx_num);
                    const auto& my_symbol = instrument(doc_view, tx_num);
                    if(ignoreTx || tx_num == 0) continue;
                    const auto& tx = tx_iter->get_document().view();
                    for(auto& tx_obj: tx){
                        process_tx_obj(my_symbol, tx_obj);
                    }
                }
                LDEBUG( "iterated through cursor");
            }

            LDEBUG( "about to finish");
            onFinish(context, symbol);
            LDEBUG( "finished");
            delete projection;
            LDEBUG( "projection freed");
        });
    }
};


IDataStorage *create_cloud_instance(OnDone onInitDone, void* caller_provided_param) {
    return new Storage<MongoDbDao>(onInitDone, caller_provided_param);
}