#include <aws/core/Aws.h>
#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/model/AttributeDefinition.h>
#include <aws/dynamodb/model/QueryRequest.h>

#include <memory>
#include <set>
#include <algorithm>
#include <string>

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
    const char db_sub_stock [] = "I#S";
    const char db_sub_fund  [] = "I#F";
    const char db_sub_tx_prefix[] = "x#";
    const char db_name_attr[] = "name";
    const char db_sub_attr[] = "sub";

    typedef Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue> AttrValueMap;
    std::function<void(AttrValueMap &)> NOOP = [](AttrValueMap &) {};

    typedef std::function<bool/*return true to continue to retrieve the next row*/(bool /*if this is the last one*/, const AttrValueMap &)> OnItem;
    typedef std::function<void(int)> OnItemCount;

    typedef std::function<void(AttrValueMap &)> AddAttrValue;

    // the strings are not sorted so binary search is not an option, but good enough given the small size of the string array
    bool find_match_str(const char** head, int num, const char* find){
        char **p = const_cast<char**>(head);
        for(int i ; i < num ;++i, ++p){
            if(strcmp(*p, find) == 0) return true;
        }
        return false;
    }
}

class AwsDao
{
private:
    Aws::SDKOptions options;
    Aws::DynamoDB::DynamoDBClient *db;
    Aws::Utils::Logging::LogSystemInterface* logger;
private:
    void log_attrs(Aws::Utils::Logging::LogLevel lvl, const char *msg, const AttrValueMap& attrs) const{
        logger->Log(Aws::Utils::Logging::LogLevel::Info, log_tag, msg);
        for(const auto& i : attrs){
            logger->Log(lvl,log_tag, "key=%s value=%s", i.first.c_str(), i.second.GetS().c_str());
        }
    }
    const void db_query(const char *index,
                        const char *key_condition_expr, Aws::Map<Aws::String, Aws::String> &attr_names,
                        const char *filter_expr,
                        AttrValueMap &expr_attr_values,
                        OnItem onItem,
                        OnItemCount onItemCount,
                        AddAttrValue add_filter_attr_value = NOOP)
    {

        Aws::DynamoDB::Model::QueryRequest q;
        q.SetTableName(dynamo_db_table);
        if (index != nullptr)
            q.SetIndexName(index);

        q.SetKeyConditionExpression(key_condition_expr);
        q.SetExpressionAttributeNames(std::move(attr_names));

        if (filter_expr != nullptr)
            q.SetFilterExpression(filter_expr);

        add_filter_attr_value(expr_attr_values);
        q.SetExpressionAttributeValues(std::move(expr_attr_values));

        AttrValueMap* lastKey = nullptr;

        while(true){
            if(lastKey != nullptr){
                log_attrs(Aws::Utils::Logging::LogLevel::Info, "querying with last key", *lastKey);
                q.SetExclusiveStartKey(std::move(*lastKey));
            }

            const auto &res = db->Query(q);
            if (!res.IsSuccess())
                throw std::runtime_error(res.GetError().GetMessage());
            const auto &result = res.GetResult();
            const auto& lk = result.GetLastEvaluatedKey();


            onItemCount(result.GetCount());
            int itemIdx = 0;
            for (const auto &item : result.GetItems())
            {
                if (!onItem(lk.empty() && (++itemIdx == result.GetCount()), item))
                    break;
            }
            if(lk.empty()) break;
            lastKey = const_cast<AttrValueMap*>(&lk);
            log_attrs(Aws::Utils::Logging::LogLevel::Info, "result has last key", lk);
        }
    }

    void db_query_by_partition_key(const char *index_name,
                                   const char *key_condition_expr,
                                   const char *key_name, const char *key_name_v,
                                   const char *value_name, const char *value,
                                   const char *filter_expr,
                                   OnItem onItem,
                                   OnItemCount onItemCount,
                                   AddAttrValue add_filter_attr_value = NOOP)
    {

        Aws::Map<Aws::String, Aws::String> attr_names;
        attr_names.emplace(key_name, key_name_v);

        Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue> attributeValues;
        attributeValues.emplace(value_name, value);

        db_query(index_name, key_condition_expr, attr_names, filter_expr, attributeValues, onItem, onItemCount, add_filter_attr_value);
    }

    inline void db_query_by_name(const char *name, const char *filter_expr, OnItem onItem, OnItemCount onItemCount, AddAttrValue add_filter_attr_value = NOOP)
    {
        db_query_by_partition_key(nullptr, "#name_n = :name_v", "#name_n", db_name_attr, ":name_v", name, filter_expr, onItem, onItemCount, add_filter_attr_value);
    }

    inline void db_query_by_sub(const char *sub, const char *filter_expr, OnItem onItem, OnItemCount onItemCount, AddAttrValue add_filter_attr_value = NOOP)
    {
        db_query_by_partition_key(sub_name_idx, "#sub_n = :sub_v", "#sub_n", db_sub_attr, ":sub_v", sub, filter_expr, onItem, onItemCount, add_filter_attr_value);
    }

    template <typename T>
    inline void db_query_by_sub_with_total_num_aware_builder(const char *sub, const char *filter_expr, Builder<T>* builder, std::function<bool(const AttrValueMap&)> onItem,  
                                                             AddAttrValue add_filter_attr_value = NOOP){
         db_query_by_sub(sub, filter_expr, [builder, &onItem](bool, const auto &item){ 
                if(!onItem(item)) return true;

                builder->alloc->inc_counter();
                if(builder->alloc->has_enough_counter()){
                    builder->succeed();
                    return false;
                }
                else return true; 
            },
            [](int){}, add_filter_attr_value
        );
    }

    inline  void get_items_by_sub_key(const char* sub_key_value, const char *filter_expr, OnItemCount totalNum, OnItem onItem, AddAttrValue add_filter_attr_value = NOOP){
        db_query_by_sub(
            sub_key_value, filter_expr, 
            [&onItem](bool is_last, const auto &item){
                onItem(is_last, item);
                return true; 
            },
            [&totalNum](int count){ totalNum(count); });
    }

    void db_query_by_name_and_sub(const char *index_name, const char *key_condition_expr,
                                  const Aws::String &name, const Aws::String &sub,
                                  const char *filter_expr,
                                  OnItem onItem, OnItemCount onItemCount,
                                  AddAttrValue add_filter_attr_value = NOOP)
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
        logger = Aws::Utils::Logging::GetLogSystem();
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

    typedef AttrValueMap BrokerType;
    void get_broker_by_name(const char *name, std::function<void(const BrokerType &)> onBrokerData)
    {
        db_query_by_name_and_sub(
            sub_name_idx, nullptr, name, db_sub_broker, nullptr, [&onBrokerData](bool is_last, const auto &item)
            { onBrokerData(item);return false; },
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

            db_query_by_name_and_sub(
                nullptr, nullptr, name, Aws::String(db_sub_broker + funds_update_date), nullptr,
                [&onBrokerBuilder, &all_ccys, &addCash, &funds_update_date](bool last_item, const auto &item)
                {
                    const auto &funds = item.at("active_funds").GetSS();
                    auto b = BrokerBuilder(all_ccys.size(), funds.size());
                    b.set_fund_update_date(funds_update_date);
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
                                return true;
                             });
    }

    void get_funds(FundsBuilder *builder, int funds_num, char* fund_update_date, const char ** fund_names_head) {
        const std::string& key = std::string(db_sub_tx_prefix) + fund_update_date;
        db_query_by_sub_with_total_num_aware_builder(key.c_str(), "attribute_exists(capital)", // only fund tx has capital attr
            builder,
            [builder, fund_names_head, funds_num](const auto &item){
                    const auto& name = item.at("name").GetS();
                    if(find_match_str(fund_names_head, funds_num, name.c_str())) return false;

                    const auto& broker = item.at("broker").GetS();
                    const int amt = std::stoi(item.at("amount").GetN());
                    const double capital = std::stod(item.at("capital").GetN());
                    const double market_value = std::stod(item.at("market_value").GetN());
                    const double price = std::stod(item.at("price").GetN());
                    const double profit = market_value - capital;
                    const double roi = profit / capital;
                    const timestamp date = std::stol(item.at("date").GetN());
                    LOG(DEBUG) << "got tx of broker " << broker << " on epoch=" << date << " fund="<<name << "\n";
                    builder->add_fund(
                        broker, name,
                        amt,
                        capital,
                        market_value,
                        price,
                        profit,
                        roi,
                        date
                    );
                    return true;
            });
    }

    inline void get_known_stocks(OnStrings onStrings, void *ctx) {
        get_non_fund_symbols([onStrings, ctx](auto* str){ onStrings(str, ctx);});
    }
    void get_non_fund_symbols(std::function<void(Strings *)> onResult) {
        StringsBuilder *sb = nullptr;
        get_items_by_sub_key(db_sub_stock, nullptr, 
            [&sb](int num) { sb = new StringsBuilder(num); },
            [&sb, &onResult](bool is_last, auto const& item){
                sb->add(item.at(db_name_attr).GetS());
                if(is_last){
                    onResult(sb->strings);
                    delete sb;
                }
                return true;
            }
        );
    }
    void get_latest_quotes(LatestQuotesBuilder *builder, int num, const char **symbols_head) {
        db_query_by_sub_with_total_num_aware_builder(db_sub_stock, "attribute_exists(last_price)", 
            builder,
            [symbols_head, num, builder](const auto &item){
                const auto& name = item.at("name").GetS();
                if(find_match_str(symbols_head, num, name.c_str())) return false;
                
                const double price = std::stod(item.at("last_price").GetN());
                const timestamp dt = std::stol(item.at("last_price_time").GetN());
                builder->add_quote(name, dt, price);
                return true;
            }
        );
    }
    void add_tx(const char *broker, const char *symbol, double shares, double price, double fee, const char *side, timestamp date,
                OnDone onDone, void *caller_provided_param) {}
    void get_stock_portfolio(StockPortfolioBuilder *builder, const char *broker, const char *symbol) {}

private:
    inline void get_all_broker_items(OnItemCount totalNum, OnItem onBrokerItem)
    {
        get_items_by_sub_key(db_sub_broker, nullptr, totalNum, onBrokerItem);
    }
 };

IDataStorage *create_cloud_instance(OnDone onInitDone) { return new Storage<AwsDao>(onInitDone); }