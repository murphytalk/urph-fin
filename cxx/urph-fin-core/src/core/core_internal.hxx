#ifndef URPH_CORE_INTERNAL_HXX_
#define URPH_CORE_INTERNAL_HXX_

#include <string>
#include <set>
#include <condition_variable>
#include "urph-fin-core.hxx"

//// Overview calculation Start
class StockPortfolio;
class AssetItem
{
public:
    AssetItem(const char* asset_type, const char* b, const char* ccy, double v, double p)
        :broker(std::string(b)),currency(std::string(ccy))  
    {
        this->asset_type = asset_type;
        this->value = v;
        this->profit = p;
    }
    AssetItem(const char* asset_type, std::string& broker, const char* ccy, double v, double p)
        :currency(std::string(ccy))  
    {
        this->broker = std::move(broker);
        this->asset_type = asset_type;
        this->value = v;
        this->profit = p;
    }
     AssetItem(AssetItem&& other)
    {
        this->asset_type = other.asset_type;
        this->broker = std::move(other.broker);
        this->currency = std::move(other.currency);
        this->value = other.value;
        this->profit = other.profit;
    }
    AssetItem(const AssetItem&) = default;

    friend bool operator == (const AssetItem& a, const AssetItem& b) { 
        return strcmp(a.asset_type, b.asset_type) == 0 && a.broker == b.broker && a.currency == b.currency &&
            a.value == b.value && a.profit == b.profit;
    };

    AssetItem& operator=(const AssetItem& t) = default;

    const char* asset_type;
    std::string broker;
    std::string currency;
    double value;
    double profit;
};

using AssetItems = std::vector<AssetItem>;

class AllAssets
{
public:
    enum Loaded{
        None = 0,
        Quotes = 1,
        Brokers = 2,
        Stocks = 4,
        Funds = 8
    };
    explicit AllAssets(std::function<void()> onLoaded);
    // for unit tests
    AllAssets(QuoteBySymbol& quotes, AllBrokers *brokers, FundPortfolio* fp, StockPortfolio* sp);
    ~AllAssets();

    void load();
    void notify(Loaded loadedData);

    double to_main_ccy(double value, const char* ccy, const char* main_ccy);

    const Quote* get_latest_quote(const char* symbol);
    std::set<std::string> get_all_ccy();
    std::set<std::string> get_all_ccy_pairs();

    AssetItems items;
private:
    std::function<void()> notifyLoaded;
    char load_status = Loaded::None;

    double get_price(const char* symbol);
    void load_funds(FundPortfolio* fp);
    void load_stocks(StockPortfolio* sp);
    void load_cash(AllBrokers *brokers);
    std::condition_variable cv;
    QuoteBySymbol* quotes_by_symbol;

    ::Quotes* q;
    StockPortfolio *stocks;
    FundPortfolio *funds;

    constexpr bool all_loaded(char status) const{
        return status == (AllAssets::Loaded::Brokers | AllAssets::Loaded::Funds | AllAssets::Loaded::Quotes | AllAssets::Loaded::Stocks);
    }

    AllAssets(const AllAssets&) = delete;
    AllAssets(AllAssets&) = delete;
    AllAssets(const AllAssets&&) = delete;
    AllAssets(AllAssets&&) = delete;
};


#endif
