#include <aws/core/Aws.h>
#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/model/AttributeDefinition.h>
#include <aws/dynamodb/model/QueryRequest.h>

#include <memory>

#include "../src/utils.hxx"
#include "../src/storage/storage.hxx"
#include "../src/core/stock.hxx"
#include "../src/core/core_internal.hxx"

namespace
{
    const char log_tag[] = "urph-fin";
    const char dynamo_db_table[] = "urph-fin";
    const char aws_region[] = "ap-northeast-1";
    const char sub_name_idx[] = "sub-name-index";

    const char db_sub_broker[] = "B#";
    const char db_name_attr[] = "name";
    const char db_sub_attr[] = "sub";

    typedef Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue> AttrValueMap;
    std::function<void(AttrValueMap &)> NOOP = [](AttrValueMap &) {};

    typedef std::function<bool(bool /*if this is the last one*/, const AttrValueMap &)> OnItem;
    typedef std::function<void(int)> OnItemCount;
}

class AwsDao
{
private:
    Aws::SDKOptions options;
    Aws::DynamoDB::DynamoDBClient *db;

private:
    const void db_query(const char *index,
                        const char *key_condition_expr, Aws::Map<Aws::String, Aws::String> &attr_names,
                        const char *filter_expr,
                        AttrValueMap &expr_attr_values,
                        OnItem onItem,
                        OnItemCount onItemCount,
                        std::function<void(AttrValueMap &)> add_filter_attr_value = NOOP)
    {

        Aws::DynamoDB::Model::QueryRequest q;
        q.SetTableName(dynamo_db_table);
        if (index != nullptr)
            q.SetIndexName(index);

        q.SetKeyConditionExpression(key_condition_expr);
        q.SetExpressionAttributeNames(std::move(attr_names));

        if (filter_expr != nullptr)
            q.SetFilterExpression(filter_expr);

        Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue> attributeValues;
        q.SetExpressionAttributeValues(std::move(expr_attr_values));

        add_filter_attr_value(expr_attr_values);

        const auto &res = db->Query(q);
        if (!res.IsSuccess())
            throw std::runtime_error(res.GetError().GetMessage());
        const auto &result = res.GetResult();

        onItemCount(result.GetCount());
        int itemIdx = 0;
        for (const auto &item : result.GetItems())
        {
            if (!onItem(++itemIdx == result.GetCount(), item))
                break;
        }
    }

    inline void db_query_by_partition_key(const char *index_name,
                                          const char *key_condition_expr,
                                          const char *key_name, const char *key_name_v,
                                          const char *value_name, const char *value,
                                          const char *filter_expr,
                                          OnItem onItem,
                                          OnItemCount onItemCount,
                                          std::function<void(AttrValueMap &)> add_filter_attr_value = NOOP)
    {

        Aws::Map<Aws::String, Aws::String> attr_names;
        attr_names.emplace(key_name, key_name_v);

        Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue> attributeValues;
        attributeValues.emplace(value_name, value);

        db_query(index_name, key_condition_expr, attr_names, filter_expr, attributeValues, onItem, onItemCount, add_filter_attr_value);
    }

    inline void db_query_by_name(const char *name, const char *filter_expr, OnItem onItem, OnItemCount onItemCount, std::function<void(AttrValueMap &)> add_filter_attr_value = NOOP)
    {
        db_query_by_partition_key(nullptr, "#name_n = :name_v", "#name_n", db_name_attr, ":name_v", name, filter_expr, onItem, onItemCount, add_filter_attr_value);
    }

    inline void db_query_by_sub_req(const char *sub, const char *filter_expr, OnItem onItem, OnItemCount onItemCount, std::function<void(AttrValueMap &)> add_filter_attr_value = NOOP)
    {
        db_query_by_partition_key(sub_name_idx, "#sub_n = :sub_v", "#sub_n", db_sub_attr, ":sub_v", sub, filter_expr, onItem, onItemCount, add_filter_attr_value);
    }

    inline void db_query_by_name_and_sub_req(const char *index_name, const char *key_condition_expr,
                                             const Aws::String &name, const Aws::String &sub,
                                             const char *filter_expr,
                                             OnItem onItem, OnItemCount onItemCount,
                                             std::function<void(AttrValueMap &)> add_filter_attr_value = NOOP)
    {
        Aws::Map<Aws::String, Aws::String> attr_names;
        attr_names.emplace("#name_n", db_name_attr);
        attr_names.emplace("#sub_n", db_sub_attr);

        Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue> attributeValues;
        attributeValues.emplace(":name_v", name);
        attributeValues.emplace(":sub_v", sub);

        db_query(index_name,
                 key_condition_expr == nullptr ? "#name_n = :name_v AND #sub_n = :sub_v" : key_condition_expr,
                 attr_names, filter_expr, attributeValues, onItem, onItemCount, add_filter_attr_value);
    }

public:
    AwsDao(OnDone onInitDone)
    {
#if !defined(__ANDROID__)
        options.loggingOptions.logLevel = Aws::Utils::Logging::LogLevel::Info;
#endif
        Aws::InitAPI(options);
        auto *logger = Aws::Utils::Logging::GetLogSystem();
        logger->Log(Aws::Utils::Logging::LogLevel::Info, log_tag, "Initialized API");

        Aws::Client::ClientConfiguration clientConfig;
        clientConfig.region = aws_region;
        logger->Log(Aws::Utils::Logging::LogLevel::Info, log_tag, "Initializing DB client");
        db = new Aws::DynamoDB::DynamoDBClient(clientConfig);
        logger->Log(Aws::Utils::Logging::LogLevel::Info, log_tag, "Initialized DB client");
        onInitDone(db);
    }
    ~AwsDao()
    {
        delete db;
        Aws::ShutdownAPI(options);
    }

    typedef Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue> BrokerType;
    void get_broker_by_name(const char *name, std::function<void(const BrokerType &)> onBrokerData)
    {
        db_query_by_name_and_sub_req(
            sub_name_idx, nullptr, name, db_sub_broker, nullptr, [&onBrokerData](bool is_last, const auto &item)
            {
            onBrokerData(item);
            return false; },
            [](int count)
            { if(count == 0) throw std::runtime_error("no such broker"); });
    }

    std::string get_broker_name(const BrokerType &broker_query_result)
    {
        const auto &v = broker_query_result.at(Aws::String(db_name_attr));
        const auto &n = v.GetS();
        return n;
    }

    void get_broker_cash_balance_and_active_funds(const BrokerType &broker_query_result, std::function<void(const BrokerBuilder &)> onBrokerBuilder)
    {
        const auto &name = get_broker_name(broker_query_result);
        LOG(DEBUG) << "Broker " << name;

        const auto &cash = broker_query_result.at(Aws::String("cash"));
        const auto &all_ccys = cash.GetM();

        auto addCash = [&all_ccys](BrokerBuilder &b)
        {
            for (const auto &c : all_ccys)
            {
                auto balance = std::stod(c.second->GetN());
                LOG(DEBUG) << "  " << c.first << " " << balance << "\n";
                b.add_cash_balance(c.first, balance);
            }
        };

        auto onlyAddCash = [&all_ccys, &addCash]()
        {
            auto b = BrokerBuilder(all_ccys.size(), 0);
            addCash(b);
        };

        auto pos = broker_query_result.find(Aws::String("funds_update_date"));
        if (pos != broker_query_result.end())
        {
            const auto &funds_update_date = pos->second.GetS();
            LOG(DEBUG) << " last funds update date: " << funds_update_date << "\n";

            db_query_by_name_and_sub_req(
                nullptr, nullptr, name, Aws::String(db_sub_broker + funds_update_date), nullptr,
                [&onBrokerBuilder, &all_ccys, &addCash](bool last_item, const auto &item)
                {
                    const auto &funds = item.at("active_funds").GetSS();
                    auto b = BrokerBuilder(all_ccys.size(), funds.size());
                    addCash(b);
                    for (const auto &f : funds)
                    {
                        b.add_active_fund(f);
                    }
                    onBrokerBuilder(b);
                    return true;
                },
                [&onlyAddCash](int count){ if(count == 0) onlyAddCash(); });
        }
        else
        {
            LOG(DEBUG) << " no funds update date or the list is empty\n";
            onlyAddCash();
        }
    }

    void get_brokers(std::function<void(AllBrokerBuilder<AwsDao, BrokerType>*)> onAllBrokersBuilder)
    {
        AllBrokerBuilder<AwsDao, BrokerType> *all = nullptr;

        get_all_broker_items([&](int n)
                             { all = new AllBrokerBuilder<AwsDao, BrokerType>(n); },
                             [&all, this, &onAllBrokersBuilder](bool is_last, const BrokerType &item)
                             {
                                LOG(DEBUG) << "adding broker , is last=" << is_last << "\n";                                
                                all->add_broker(this, item);
                                if(is_last) onAllBrokersBuilder(all);
                             });
    }

    void get_funds(FundsBuilder *, int, const char **) {}
    StringsBuilder get_known_stocks() { return StringsBuilder(0); }
    void get_non_fund_symbols(std::function<void(Strings *)> onResult) {}
    void get_latest_quotes(LatestQuotesBuilder *builder, int num, const char **symbols_head) {}
    void add_tx(const char *broker, const char *symbol, double shares, double price, double fee, const char *side, timestamp date,
                OnDone onDone, void *caller_provided_param) {}
    void get_stock_portfolio(StockPortfolioBuilder *builder, const char *broker, const char *symbol) {}

private:
    void get_all_broker_items(std::function<void(int)> totalNum, std::function<void(bool, const BrokerType &)> onBrokerItem)
    {
        db_query_by_sub_req(
            db_sub_broker, nullptr, [&onBrokerItem](bool is_last, const auto &item)
            {
            onBrokerItem(is_last, item);
            return true; },
            [&totalNum](int count)
            { totalNum(count); });
    }
};

IDataStorage *create_cloud_instance(OnDone onInitDone) { return new Storage<AwsDao>(onInitDone); }