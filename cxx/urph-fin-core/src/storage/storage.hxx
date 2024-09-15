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
#include <limits>


#include "../utils.hxx"
#include "../core/urph-fin-core.hxx"
#include "../core/stock.hxx"

template <typename T>
class Builder : public NonCopyableMoveable{
public:
    typedef PlacementNew<T> Alloc;
    Alloc* alloc;

    Builder() = delete;
    // the only way to instantiate is to call create() to allocate in heap
    // this will be cast to child class , so child class cannot have any additional data members
    static Builder<T>* create(int num, std::function<void(Alloc*)> called_when_succeed){
        return new Builder<T>(num, called_when_succeed);
    }

   ~Builder(){
        delete alloc;
    }
    inline void succeed() {
        LDEBUG( "Builder succeeded");
        onSuccess(alloc);
        delete this;
    }
    inline void failed(){
        LDEBUG( "Builder failed");
        delete this;
    }
protected:
    std::vector<T> v;
    std::function<void(Alloc*)> onSuccess;
    // ensure this cannot be allocated in stack
    Builder(int num, std::function<void(Alloc*)> called_when_succeed){
        alloc = new PlacementNew<T>(num);
        onSuccess = called_when_succeed;
    }
};

class StringsBuilder{
public:
    Strings *strings;
    StringsBuilder(int n){
        strings = new Strings(n);
    }
    void add(const std::string_view& str){
        strings->add(str);
    }
};

class BrokerBuilder{
public:
    typedef PlacementNew<cash_balance> CashBalanceAlloc;
    CashBalanceAlloc * balances_alloc;
    StringsBuilder *funds_name_builder;
    char* fund_update_date;
    BrokerBuilder(int n, int active_fund_num){
        balances_alloc = new CashBalanceAlloc(n);
        funds_name_builder = active_fund_num > 0 ? new StringsBuilder(active_fund_num) : nullptr;
        fund_update_date = nullptr;
    }
    ~BrokerBuilder(){
        delete balances_alloc;
        delete funds_name_builder;
    }
    void set_fund_update_date(const std::string_view& yyyymmdd){
        fund_update_date = copy_str(yyyymmdd);
    }
    void add_cash_balance(const std::string_view& currency, double balance){
        LDEBUG( "  " << currency << " " << balance);
        auto * p = new (balances_alloc->next()) CashBalance(currency, balance);
        LDEBUG( "CashBalance ccy=" << p->ccy << " " << p->balance);
    }
    void add_active_fund(const std::string_view& active_fund_id){
        LDEBUG( "adding fund " << active_fund_id);
        funds_name_builder->add(active_fund_id);
    }
};

template<typename DAO, typename BrokerType>
static void create_broker(
    DAO* dao,
    const BrokerType& brokerQueryResult,
    std::function<Broker*(const std::string_view&/*name*/, int/*ccy_num*/, cash_balance* /*first_ccy_balance*/, char* /*yyyymmdd: fund update date*/, strings* /*active_fund_ids*/)> create_func,
    std::function<void(Broker*)> onBrokerCreated)
{
    dao->get_broker_cash_balance_and_active_funds(brokerQueryResult,
        [dao,&brokerQueryResult, &create_func, &onBrokerCreated](const BrokerBuilder& builder){
            onBrokerCreated(
                create_func(
                    dao->get_broker_name(brokerQueryResult), 
                    builder.balances_alloc->allocated_num(), 
                    builder.balances_alloc->head(), 
                    builder.fund_update_date, 
                    builder.funds_name_builder == nullptr ? nullptr: builder.funds_name_builder->strings
                )
            );
        }
    );
}

template<typename DAO, typename BrokerType>
class AllBrokerBuilder{
public:
    typedef PlacementNew<broker> BrokerAlloc;
    BrokerAlloc *alloc;
    AllBrokerBuilder(int n){
        LDEBUG( "Total brokers:" << n);
        alloc = new BrokerAlloc(n);
    }
    ~AllBrokerBuilder(){
        delete alloc;
    }
    void add_broker(DAO* dao, const BrokerType& b){
        create_broker(dao, b, [&](const std::string_view&n, int ccy_num, cash_balance* first_ccy_balance, char* fund_update_date, strings* active_funds){
            LDEBUG( "creating broker " << n);
#ifdef _MSC_VER
            void* p = all_brokers++;
            return new (p)
#else
            return new (alloc->next())
#endif
                Broker(n, ccy_num, first_ccy_balance, fund_update_date, active_funds);
        }, [](Broker* broker){ LDEBUG( "broker created"); });
    }
};

class FundsBuilder: public Builder<fund>{
public:
    Fund* add_fund(const std::string_view& broker,  const std::string_view& name,  int amount, double capital, double market_value, double price, double profit, double roi, asset_class_ratio&& ratios, timestamp date){
        return new (alloc->next()) Fund(broker, name,
                                        amount,
                                        capital,
                                        market_value,
                                        price,
                                        profit,
                                        roi,
                                        std::move(ratios),
                                        date
        );
    }
};

class LatestQuotesBuilder: public Builder<quote>{
public:
    Quote* add_quote(const std::string& symbol, timestamp date, double rate){
        LDEBUG( "Quote sym=" << symbol << ", date=" << date << ", rate=" << rate );
        return new (alloc->next()) Quote(symbol, date, rate);
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
    Stock* add_stock(const std::string_view& symbol, const std::string_view& ccy, asset_class_ratio& ratio){
        LDEBUG( "adding stock " << symbol << "@" << ccy);
        return new (stock_alloc->next()) Stock(symbol, ccy, std::move(ratio));
    }
    void prepare_stock_alloc(int n) {
        LDEBUG( "Got " << n << " stocks");
        unfinished_stocks = n;
        stock_alloc = new StockAlloc(n);
    }
    void prepare_stock_alloc_dont_know_total_num(int n) {
        LDEBUG( "Assuming " << n << " stocks");
        unfinished_stocks = -1;
        stock_alloc = new StockAlloc(n);
    }
     void prepare_tx_alloc(const std::string& symbol, int num){
        LDEBUG( "Got " << num << " tx for " << symbol);
        if(num == 0 && unfinished_stocks >= 0){
            check_completion(nullptr);
        }
        else tx[symbol] = new TxAlloc(num);
    }
    void rm_stock(const std::string& symbol){
        auto it = tx.find(symbol);
        if(it!=tx.end()){
            delete it->second;
            tx.erase(it);
        }
        --unfinished_stocks;
    }
    void addTx(const std::string_view& broker,const std::string_view& symbol, const std::string_view& type, const double price, const double shares, const double fee, const timestamp date){
        LDEBUG( "Adding tx@" << date << " for " << symbol << " broker=" << broker << ",type=" << type << ",price=" << price << ",shares=" << shares);
        const auto s = tx.find(std::string(symbol));
        if(s != tx.end()){
            const auto& tx_alloc = s->second;
            new (tx_alloc->next()) StockTx(broker, shares, price, fee, type, date);
            LDEBUG( "Added tx@" << date << " for " << symbol << " broker=" << broker);
            if(unfinished_stocks >= 0){
                // only calls this when we know the extact stock num in advance
                check_completion(tx_alloc);
            }
        }
        else throw std::runtime_error("Cannot find tx for stock " + std::string(symbol));
    }
    void incr_counter(const std::string& symbol){
        LDEBUG( "Increasing counter for " << symbol);
        const auto s = tx.find(symbol);
        if(s != tx.end()){
            auto counter = s->second->inc_counter();
            LDEBUG( "Increased counter for " << symbol << " to " << counter );
        }
    }
    inline void failed(){
        delete this;
    }

    void complete(){
        onSuccess(stock_alloc, tx);
        delete this;
    }

    static stock_with_tx* create_stock_with_tx(StockAlloc* stock_alloc, const StockPortfolioBuilder::TxAllocPointerBySymbol& tx)
    {
        int stock_num = stock_alloc->allocated_num();
        stock_with_tx* head = new stock_with_tx[stock_num];
        memset(head, 0, sizeof(stock_with_tx) * stock_num);
        stock_with_tx* current = head;
        for(auto* b = stock_alloc->head(); b != stock_alloc->end(); ++b){
            auto tx_iter = tx.find(b->symbol);
            int tx_num;
            stock_tx* first;
            if(tx_iter == tx.end()){
                tx_num = 0;
                first = nullptr;
            }
            else{
                tx_num = tx_iter->second->allocated_num();
                first = tx_iter->second->head();
            }
            new (current++) StockWithTx(b, new StockTxList(tx_num, first));
        }
        return head;
    }
private:
    //https://stackoverflow.com/questions/1394132/macro-and-member-function-conflict
    int unfinished_stocks = (std::numeric_limits<int>::max)();

    TxAllocPointerBySymbol tx;
    OnSuccess onSuccess;
    StockPortfolioBuilder(OnSuccess on_success){
        stock_alloc = nullptr;
        onSuccess = on_success;
    }
    void check_completion(const TxAlloc* alloc){
        if(alloc!=nullptr){
            LDEBUG( "Checking completion: cur =" << alloc->counter() << ", max=" << alloc->max_counter());
        }
        if(alloc == nullptr || alloc->has_enough_counter()){
            --unfinished_stocks;
            LDEBUG( "Enough tx , remaining stocks = " << unfinished_stocks);
            if(unfinished_stocks == 0){
                // no more pending
                complete();
            }
        }
    }
};

struct FundsParam{
    std::string broker;
    std::string name;
    std::string update_date;
    FundsParam(char* b, char* n, char* ud):broker(b),name(n), update_date(ud){}
};

class IDataStorage{
public:
    virtual ~IDataStorage(){}
    virtual void get_broker(const char* name, OnBroker onBroker, void*param) = 0;
    virtual void get_brokers(OnAllBrokers onAllBrokers, void* param) = 0;
    virtual void get_funds(std::vector<FundsParam>& params, OnFunds onFunds, void* onFundsCallerProvidedParam,const std::function<void()>& clean_func) = 0;
    virtual void get_stock_portfolio(const char* broker, const char* symbol, OnAllStockTx onAllStockTx, void* caller_provided_param) = 0;
    virtual void get_known_stocks(OnStrings onStrings, void *ctx) = 0;
    virtual void get_quotes(int num, const char **symbols_head, OnQuotes onQuotes, void* caller_provided_param) = 0;
    virtual void add_tx(const char* broker, const char* symbol, double shares, double price, double fee, const char* side, timestamp date,
                OnDone onDone,void* caller_provided_param) = 0;
    virtual void update_cash(const char* broker, const char* ccy, double balance,OnDone onDone,void* caller_provided_param) = 0;
};

template<typename DAO>
class Storage: public IDataStorage{
    std::unique_ptr<DAO> dao;
public:
    Storage(OnDone onInitDone, void* caller_provided_param){
        dao = std::make_unique<DAO>(onInitDone, caller_provided_param);
        LDEBUG( "Storage instance created");
    };
    Storage(DAO *p): dao(std::unique_ptr<DAO>(p)){
        LDEBUG( "Storage instance created");
    };
    virtual ~Storage(){
        LDEBUG( "Storage instance freed");
    }

    void get_broker(const char* name, OnBroker onBroker, void*param)
    {
        dao->get_broker_by_name(
            name,
            [this, &onBroker, param](const auto& brokerQueryResult) {
                create_broker(dao.get(),
                    brokerQueryResult,
                    [](const std::string_view&n, int ccy_num, cash_balance* first_ccy_balance, char* fund_update_date, strings* active_funds){
                        return new Broker(n, ccy_num, first_ccy_balance, fund_update_date, active_funds);
                    },
                    [&onBroker,param](Broker* b){ onBroker(b, param);}
                );
            }
        );
    }

    void get_brokers(OnAllBrokers onAllBrokers, void* param){
        // capturing by value as it is going to be executed in another thread
        dao->get_brokers([=](auto *p){
            AllBrokers * b = new AllBrokers(p->alloc->allocated_num(), p->alloc->head());
            delete p;
            onAllBrokers(b, param);
        });
    }

    void get_funds(std::vector<FundsParam>& params, OnFunds onFunds, void* onFundsCallerProvidedParam,const std::function<void()>& clean_func){
        // self delete upon finish
        auto *p = static_cast<FundsBuilder*>(FundsBuilder::create(params.size(),[onFunds, onFundsCallerProvidedParam,clean_func](FundsBuilder::Alloc* fund_alloc){
            std::sort(fund_alloc->head(), fund_alloc->head() + fund_alloc->allocated_num(),[](fund& f1, fund& f2){
                auto byBroker = strcmp(f1.broker,f2.broker);
                auto v = byBroker == 0 ? strcmp(f1.name, f2.name) : byBroker;
                return v < 0;
            });
            onFunds(new FundPortfolio(fund_alloc->allocated_num(), fund_alloc->head()), onFundsCallerProvidedParam);
            clean_func();
        }));
        dao->get_funds(p, std::move(params));
    }

    void get_stock_portfolio(const char* broker, const char* symbol, OnAllStockTx onAllStockTx, void* caller_provided_param){
        // self delete upon finish
        auto *builder =
            StockPortfolioBuilder::create([onAllStockTx, caller_provided_param](StockPortfolioBuilder::StockAlloc* stock_alloc, const StockPortfolioBuilder::TxAllocPointerBySymbol& tx){
                const auto stock_num = stock_alloc->allocated_num();
                auto *stock_with_tx_head = StockPortfolioBuilder::create_stock_with_tx(stock_alloc, tx);
                onAllStockTx(new StockPortfolio(stock_num, stock_alloc->head(), stock_with_tx_head), caller_provided_param);
            });
        dao->get_stock_portfolio(builder, broker, symbol);
    }

    void get_known_stocks(OnStrings onStrings, void *ctx) {
        dao->get_known_stocks(onStrings, ctx);
    }

    void get_quotes(int num, const char **symbols_head, OnQuotes onQuotes, void* caller_provided_param){
        int preallocated_num = symbols_head == nullptr ? 10 : num;
        auto* builder = static_cast<LatestQuotesBuilder*>(LatestQuotesBuilder::create(preallocated_num,[onQuotes, caller_provided_param](LatestQuotesBuilder::Alloc* alloc){
            onQuotes(new Quotes(alloc->allocated_num(), alloc->head()), caller_provided_param);
        }));

        if (symbols_head == nullptr){
            dao->get_latest_quotes(builder);
        }
        else{
            dao->get_latest_quotes(builder, num, symbols_head);
        }
    }
    void add_tx(const char* broker, const char* symbol, double shares, double price, double fee, const char* side, timestamp date,
                OnDone onDone,void* caller_provided_param){
        dao->add_tx(broker, symbol, shares, price, fee, side, date, onDone, caller_provided_param);
    }
    void update_cash(const char* broker, const char* ccy, double balance, OnDone onDone,void* caller_provided_param){
        dao->update_cash(broker, ccy, balance,onDone, caller_provided_param);
    }
};

IDataStorage* create_cloud_instance(OnDone onInitDone, void* caller_provided_param);

#endif