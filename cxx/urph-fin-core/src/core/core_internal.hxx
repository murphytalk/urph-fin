#ifndef URPH_CORE_INTERNAL_HXX_
#define URPH_CORE_INTERNAL_HXX_

#include <string>
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
typedef std::vector<AssetItem> AssetItems;

class AllAssets
{
public:
    AllAssets();
    // for unit tests
    AllAssets(QuoteBySymbol&& quotes, AllBrokers *brokers, FundPortfolio* fp, StockPortfolio* sp);
    ~AllAssets();

    double to_main_ccy(double value, const char* ccy, const char* main_ccy);

    const Quote* get_latest_quote(const char* symbol);

    AssetItems items;
private:
    double get_price(const char* symbol);
    void load_funds(FundPortfolio* fp);
    void load_stocks(StockPortfolio* sp);
    void load_cash(AllBrokers *brokers);
    std::condition_variable cv;
    quotes* q;
    QuoteBySymbol quotes_by_symbol;

    AllAssets(const AllAssets&) = delete;
    AllAssets(AllAssets&) = delete;
    AllAssets(const AllAssets&&) = delete;
    AllAssets(AllAssets&&) = delete;
};


#endif
