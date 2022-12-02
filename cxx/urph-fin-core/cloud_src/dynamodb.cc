#include <aws/core/Aws.h>
#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/model/AttributeDefinition.h>
#include <aws/dynamodb/model/QueryRequest.h>

#include <memory>

#include "../src/utils.hxx"
#include "../src/storage/storage.hxx"
#include "../src/core/stock.hxx"
#include "../src/core/core_internal.hxx"

namespace{
    const char dynamo_db_table[] = "urph-fin";
    const char aws_region[] = "ap-northeast-1";
    const char sub_name_idx[] = "sub_name_index";

    const char db_sub_broker [] = "B#";
    const char db_name_attr  [] = "name";
    const char db_sub_attr   [] = "sub";
}


class AwsDao{
private:
    Aws::SDKOptions options;
    std::unique_ptr<Aws::DynamoDB::DynamoDBClient> db;
private:
    const Aws::DynamoDB::Model::QueryOutcome db_query_req(const char* index, const char* key_condition_expr,const char* filter_expr,
                                                          Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue>& expr_attr_values){

        Aws::DynamoDB::Model::QueryRequest q;
        q.SetTableName(dynamo_db_table);
        if(index != nullptr) q.SetIndexName(index);
        
        q.SetKeyConditionExpression(key_condition_expr);
        q.SetExpressionAttributeValues(std::move(expr_attr_values));

        if(filter_expr !=nullptr) q.SetFilterExpression(filter_expr);

        return db->Query(q);
    }

public:
    AwsDao(OnDone onInitDone){
#if !defined(__ANDROID__)
        options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Info;
#endif        
        Aws::InitAPI(options);

        Aws::Client::ClientConfiguration clientConfig;
        clientConfig.region = aws_region;
        db = std::make_unique<Aws::DynamoDB::DynamoDBClient>(clientConfig);
        onInitDone(db.get());
    }
    ~AwsDao(){
        Aws::ShutdownAPI(options);
    }

    typedef Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue> BrokerType;
    BrokerType get_broker_by_name(const char *name){
        Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue> attributeValues;
        attributeValues.emplace(":sub_v", Aws::String(db_sub_broker));
        attributeValues.emplace(":name_v", Aws::String(name));

        const auto& result = db_query_req(sub_name_idx, "sub = :sub_v AND name = :name_v", nullptr, attributeValues);
        if (result.IsSuccess()){
            const auto& items = result.GetResult().GetItems();
            auto num = items.size();
            if(num == 0) throw std::runtime_error("Cannot get broker: empty result");
            if(num > 1) LOG(WARNING) << "more than one result return for broker name=" << name << "\n";
            return items.front(); 
        }
        else throw std::runtime_error("Cannot get broker: " + result.GetError().GetMessage());
    }

    std::string get_broker_name(const BrokerType &broker_query_result) {
        std::map<std::string,int> m;
        const auto& v = broker_query_result.at(Aws::String(db_name_attr));
        const auto& n = v.GetS();
        return n;
    }

    BrokerBuilder get_broker_cash_balance_and_active_funds(const BrokerType &broker_query_result)
    {
        const auto& name = get_broker_name(broker_query_result);
        LOG(DEBUG) << "Broker " << name;

        const auto& cash = broker_query_result.at(Aws::String("cash"));
        const auto& all_ccys = cash.GetM();

        auto addCash = [&all_ccys](BrokerBuilder& b){
            for(const auto& c: all_ccys){
                auto balance = std::stod(c.second->GetN());
                LOG(DEBUG) << "  " << c.first << " " << balance << "" "\n";
                b.add_cash_balance(c.first, balance);
            }
        };

        auto pos = broker_query_result.find(Aws::String("funds_update_date"));
        if(pos != broker_query_result.end()){
            const auto& funds_update_date = pos->second.GetS();
            LOG(DEBUG) << "last funds update date: " << funds_update_date << "\n";

            Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue> attributeValues;
            attributeValues.emplace(":name_v", Aws::String(name));
            attributeValues.emplace(":sub_v", Aws::String( db_sub_broker + funds_update_date));
            const auto& result = db_query_req(nullptr, "name = :name_v AND sub = :sub_v", nullptr, attributeValues);
            if (result.IsSuccess()){
                const auto& items = result.GetResult().GetItems();

                auto b = BrokerBuilder(all_ccys.size(), items.size());
                addCash(b);

                for(const auto& f: items.front().at("active_funds").GetSS()){
                    b.add_active_fund(f);                    
                }

                return b;
            }
            else throw std::runtime_error("Cannot get broker: " + result.GetError().GetMessage());
        }
        else{
            LOG(DEBUG) << "no funds update date\n";
            auto b = BrokerBuilder(all_ccys.size(), 0);
            addCash(b);
            return b;
        }
    }


    AllBrokerBuilder<AwsDao, BrokerType> *get_brokers() {
        AllBrokerBuilder<AwsDao, BrokerType>* all = nullptr;

        get_all_broker_items([&all](int n){ all =  new AllBrokerBuilder<AwsDao, BrokerType>(n); }, [all,this](const BrokerType& item){
            all->add_broker(this, item);
        });

        return all; 
    }

    StringsBuilder *get_all_broker_names() { 
        StringsBuilder* p = nullptr;
        get_all_broker_items([&p](int n){ p =  new StringsBuilder(n); }, [p,this](const BrokerType& item){
            p->add(item.at(db_name_attr).GetS());
        });
        return p;
    }

    void get_funds(FundsBuilder *, int, const char **) {}
    StringsBuilder get_known_stocks() { return StringsBuilder(0); }
    void get_non_fund_symbols(std::function<void(Strings *)> onResult) {}
    void get_latest_quotes(LatestQuotesBuilder *builder, int num, const char **symbols_head) {}
    void add_tx(const char *broker, const char *symbol, double shares, double price, double fee, const char *side, timestamp date,
                OnDone onDone, void *caller_provided_param) {}
    void get_stock_portfolio(StockPortfolioBuilder *builder, const char *broker, const char *symbol){}
private:
    void get_all_broker_items(std::function<void(int)> totalNum, std::function<void(const BrokerType&)> onBrokerItem){
        Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue> attributeValues;
        attributeValues.emplace(":sub_v", Aws::String(db_sub_broker));
        const auto& result = db_query_req(sub_name_idx, "sub = :sub_v", nullptr, attributeValues);
        if(!result.IsSuccess()) throw std::runtime_error("Cannot get broker: " + result.GetError().GetMessage());
        const auto& items = result.GetResult().GetItems();
        totalNum(items.size());
        for(const auto& b: items){
            onBrokerItem(b);
        }
    }
};

IDataStorage * create_cloud_instance(OnDone onInitDone) { return new Storage<AwsDao>(onInitDone); }