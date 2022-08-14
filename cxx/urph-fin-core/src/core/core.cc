#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#include <pwd.h>
#endif  // _WIN32

#include <cstdlib>
#include <cstring>
#include <cassert>
#include <numeric>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <algorithm>

#include "storage/storage.hxx"
#include "urph-fin-core.hxx"
#include "stock.hxx"

// 3rd party
#include "aixlog.hpp"
#include "yaml-cpp/yaml.h"
#include "groupby.hpp"

static std::string get_home_dir()
{
    const char *homedir;
#ifdef _WIN32
    //HOMEPATH or user profile
#else
    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }
#endif
    // compiler is smart enough to either do return-value-optimization or use move
    //https://stackoverflow.com/questions/4986673/c11-rvalues-and-move-semantics-confusion-return-statement
    return std::string(homedir);
}

Config load_cfg(){
    auto cfg = YAML::LoadFile(get_home_dir() + "/.finance-credentials/urph-fin.yaml");
    auto userCfg = cfg["user"];
    return Config{userCfg["email"].as<std::string>(), userCfg["password"].as<std::string>()};
}

// dont forget to free
char* copy_str(const std::string& str)
{
    char* p = new char[str.size() + 1];
    strncpy(p, str.c_str(), str.size());
    p[str.size()] = 0;
    return p;
}

CashBalance::CashBalance(const std::string& n, float v)
{
    ccy = copy_str(n);
    balance = v;        
}

CashBalance::~CashBalance()
{
    LOG(DEBUG) << " ccy= " << ccy << " ";
    delete[] ccy;
}

Strings::Strings(int n)
{
    capacity = n;
    strs = new char* [n];    
    last_str = strs;
}

void Strings::add(const std::string& i)
{
    if(size() == capacity){
        std::stringstream ss;
        ss << "Could not add string [" << i << "] : exceeding capacity " << capacity;
        throw std::runtime_error(ss.str());
    }
    *last_str++ = copy_str(i);
}

int Strings::size()
{
    return last_str - strs;
}

char** Strings::to_str_array()
{
    char ** ids = new char*[size()];
    char ** pp = ids;
    for(char* p: *this){
        *pp++ = p;
    }
    return ids;
}

Strings::~Strings()
{
    for(char** p=strs; p!=last_str; ++p){
        LOG(DEBUG) << *p << ",";
        delete []*p;
    }
    LOG(DEBUG)<<" deleted\n";
    delete []strs;
}

void free_strings(strings* ss)
{
    delete static_cast<Strings*>(ss);
}

Broker::Broker(const std::string&n, int ccy_num, cash_balance* first_ccy_balance, strings* active_funds)
{
    name = copy_str(n);
    num = ccy_num;
    first_cash_balance = first_ccy_balance;
    active_fund_ids = active_funds;
}

Broker::~Broker(){
    LOG(DEBUG) << "freeing broker " << name << " : cash balances: [";
    delete[] name;

    free_placement_allocated_structs<Broker, CashBalance>(this);
    delete[] first_cash_balance;

    LOG(DEBUG) << "] - active funds [";
    free_strings(active_fund_ids);
    LOG(DEBUG) << "] - freed!\n";
}

AllBrokers::AllBrokers(int n, broker* broker)
{
    num = n;
    first_broker = broker;
}

AllBrokers::~AllBrokers()
{
    LOG(DEBUG) << "freeing " << num << " brokers \n";
    free_placement_allocated_structs<AllBrokers, Broker>(this);
    delete []first_broker;
}

FundPortfolio::FundPortfolio(int n, fund* f)
{
    num = n;
    first_fund = f;
}
FundPortfolio::~FundPortfolio()
{
    free_placement_allocated_structs<FundPortfolio, Fund>(this);
    delete []first_fund;
}

Fund::Fund(const std::string& b,  const std::string&n,  const std::string&i, int a, double c, double m, double prc, double p, double r, timestamp d)
{
    broker = copy_str(b);
    name = copy_str(n);
    id = copy_str(i);
    amount = a;
    capital = c;
    market_value = m;
    price = prc;
    profit = p;
    ROI = r;
    date = d;
}    

Fund::~Fund()
{
    delete []broker;
    delete []name;
    delete []id;
}

Quote::Quote(const std::string& s, timestamp t, double r)
{
    symbol = copy_str(s);
    date = t;
    rate = r;
}

Quote::~Quote()
{
    delete []symbol;
}

OverviewItem::OverviewItem(const std::string& n, double v, double v2)
{
    name = copy_str(n);
    value = v;
    value_in_main_ccy = v2;
}

OverviewItem::~OverviewItem()
{
    delete []name;
}


OverviewItemContainer::OverviewItemContainer(const std::string& n, const std::string& item_name,double sum, int num, overview_item* head)
{
    name = copy_str(n);
    this->item_name = copy_str(item_name);
    sum_in_main_ccy = sum;
    this->num = num;
    items = head;
}
OverviewItemContainer::~OverviewItemContainer()
{
    delete []name;
    delete []item_name;
    free_placement_allocated_structs<OverviewItemContainer, OverviewItem>(this);
    delete []items;
}


OverviewItemContainerContainer::OverviewItemContainerContainer(const std::string& name, const std::string& item_name, double sum_in_main_ccy, int num, overview_item_container* head)
{
    this->name = copy_str(name);
    this->item_name = copy_str(item_name);
    this->num = num;
    this->sum_in_main_ccy = sum_in_main_ccy;
    this->containers = head;
}
OverviewItemContainerContainer::~OverviewItemContainerContainer()
{
    delete []name;
    delete []item_name;
    free_placement_allocated_structs<OverviewItemContainerContainer, OverviewItemContainer>(this);
    delete []containers;
}


Overview::Overview(const std::string& item_name, int num, overview_item_container_container* head)
{
    this->item_name = copy_str(item_name);
    this->num = num;
    this->first = head;
}
Overview::~Overview()
{
    delete []item_name;
    free_placement_allocated_structs<Overview, OverviewItemContainerContainer>(this);
    delete []first;
}

IStorage *storage;

bool urph_fin_core_init(OnDone onInitDone)
{
    std::vector<AixLog::log_sink_ptr> sinks;
    
    auto log_file = getenv("LOGFILE");
    auto verbose = getenv("VERBOSE");
    auto log_lvl = verbose == nullptr ? AixLog::Severity::info : AixLog::Severity::debug;
    if(log_file == nullptr)
        sinks.push_back(std::make_shared<AixLog::SinkCout>(log_lvl));
    else  
        sinks.push_back(std::make_shared<AixLog::SinkFile>(log_lvl, log_file)); 
    AixLog::Log::init(sinks);
    
    LOG(DEBUG) << "urph-fin-core initializing\n";

    try{
        storage = create_firestore_instance(onInitDone);
        return true;
    }
    catch(const std::exception& e){
        LOG(ERROR) << "Failed to init storage service: " << e.what() << "\n";
        return false;
    }
}

void urph_fin_core_close()
{
    delete storage;
}

#define TRY try{
#define CATCH(error_ret) }catch(const std::runtime_error& e) { LOG(ERROR) << e.what(); return error_ret; }    
#define CATCH_NO_RET }catch(const std::runtime_error& e) { LOG(ERROR) << e.what(); }    

// https://stackoverflow.com/questions/60879616/dart-flutter-getting-data-array-from-c-c-using-ffi
all_brokers* get_brokers()
{
    assert(storage != nullptr);
    return storage->get_brokers();
}

void free_brokers(all_brokers* b)
{
    AllBrokers* brokers = static_cast<AllBrokers*>(b);
    delete brokers;
}

broker* get_broker(const char* name)
{
    assert(storage != nullptr);
    TRY
    return storage->get_broker(name);
    CATCH(nullptr)
}

void free_broker(broker* b)
{
    delete static_cast<Broker*>(b);
}

strings* get_all_broker_names(size_t* size)
{
    assert(storage != nullptr);
    TRY
    return storage->get_all_broker_names(*size);
    CATCH(nullptr)
}


void get_funds(int num, const char **fund_ids, OnFunds onFunds, void*param)
{
    assert(storage != nullptr);
    TRY
    storage->get_funds(num, fund_ids, onFunds, param);
    CATCH_NO_RET
}

void get_active_funds(const char* broker_name, OnFunds onFunds, void*param)
{
    Iterator<Broker> *begin, *end;
    Broker* broker =  nullptr;
    AllBrokers* all_brokers = nullptr;

    int fund_num = 0;
    std::vector<Broker*> all_broker_pointers;

    if(broker_name != nullptr){
        broker = static_cast<Broker*>(get_broker(broker_name));
        if(broker == nullptr){
            onFunds(nullptr, param);
            return;
        }
        fund_num = broker->size(Broker::active_fund_tag());
        all_broker_pointers.push_back(broker);
    }
    else{
        all_brokers = static_cast<AllBrokers*>(get_brokers());
        for(Broker& b: *all_brokers){
            fund_num += b.size(Broker::active_fund_tag());
            all_broker_pointers.push_back(&b); 
        }
    }

    const char ** ids = new const char* [fund_num];
    const char ** ids_head = ids;
    for(auto* broker: all_broker_pointers){
        for(auto it = broker->fund_begin(); it!= broker->fund_end(); ++it){
            *ids++ = *it;
        }
    }
    TRY
    get_funds(fund_num, ids_head, onFunds, param);       
    CATCH_NO_RET

    delete []ids_head;

    delete broker;
    delete all_brokers;
}

void free_funds(fund_portfolio* f)
{
    auto p = static_cast<FundPortfolio*>(f);
    delete p;
}


fund_sum calc_fund_sum(fund_portfolio* portfolio)
{
    struct fund_sum init = { 0.0, 0.0, 0.0 };

    auto r = std::accumulate(portfolio->first_fund, portfolio->first_fund + portfolio->num, init, [](fund_sum& sum, fund& f){ 
        sum.capital += f.capital; 
        sum.market_value += f.market_value; 
        sum.profit +=  f.market_value - f.capital;
        sum.ROI +=  sum.profit/sum.capital;
        return sum;
    });

    r.ROI /= portfolio->num;

    return r;
}

strings* get_known_stocks()
{
    assert(storage != nullptr);
    TRY
    return storage->get_known_stocks();
    CATCH(nullptr)
}

void get_stock_portfolio(const char* broker, const char* symbol, OnAllStockTx callback, void* caller_provided_param)
{
    assert(storage != nullptr);
    TRY
    storage->get_stock_portfolio(broker, symbol,callback, caller_provided_param);  
    CATCH_NO_RET
}

void free_stock_portfolio(stock_portfolio *p)
{
    auto port = static_cast<StockPortfolio*>(p);
    delete port;
}

stock_balance get_stock_balance(stock_tx_list* tx)
{
    auto p = static_cast<StockTxList*>(tx);
    return p->calc();
}

void get_quotes_async(int num, const char **symbols_head, OnQuotes onQuotes, void* caller_provided_param)
{
    assert(storage != nullptr);
    TRY
    storage->get_quotes(num, symbols_head, onQuotes, caller_provided_param);
    CATCH_NO_RET
}

static std::condition_variable cv;
quotes* get_quotes(int num, const char **symbols_head)
{
    quotes* all_quotes;
    std::mutex m;
    {
        std::lock_guard<std::mutex> lk(m);
        try{
            get_quotes_async(num, symbols_head,[](quotes*q, void* arg){
                quotes** all_quotes_ptr = reinterpret_cast<quotes**>(arg);
                *all_quotes_ptr = q; 
                cv.notify_one();
            }, &all_quotes);
        }
        catch(std::runtime_error& e)
        {
            LOG(ERROR)<< e.what() <<"\n";
            all_quotes = nullptr;
            cv.notify_one();
        }
    }
    {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk);
    }
    return all_quotes;
}

quotes* get_all_quotes(QuoteBySymbol& quotes_by_symbol)
{
    //todo: the async version is conflicting with get stock portfolio ...
    auto* all_quotes = get_quotes(0, nullptr);
    auto* q = static_cast<Quotes*>(all_quotes);
    for(auto const& quote: *q){
        quotes_by_symbol[quote.symbol] = &quote;
    }
    return all_quotes;
}


void free_quotes(quotes* q)
{
    auto quotes = static_cast<Quotes*>(q);
    delete quotes;
}

void add_stock_tx(const char* broker, const char* symbol, double shares, double price, double fee, const char* side, timestamp date, OnDone onDone, void*caller_provided_param)
{
    assert(storage != nullptr);
    TRY
    storage->add_tx(broker, symbol,shares, price, fee, side, date, onDone, caller_provided_param);
    CATCH_NO_RET
}

//// Overview calculation Start
#include "core_internal.hxx"

const char ASSET_TYPE_STOCK [] = "Stock&ETF";
const char ASSET_TYPE_FUNDS [] = "Funds";
const char ASSET_TYPE_CASH  [] = "Cash";

AllAssets::AllAssets(){
    q = get_all_quotes(quotes_by_symbol);

    AllBrokers *brokers = static_cast<AllBrokers*>(get_brokers());
    load_cash(brokers);
    free_brokers(brokers);

    std::mutex m;
    {
        std::lock_guard<std::mutex> lk(m);
        get_active_funds( nullptr ,[](fund_portfolio* fp, void *param){
            AllAssets *me = reinterpret_cast<AllAssets*>(param);
            auto* funds = static_cast<FundPortfolio*>(fp);
            me->load_funds(funds);
            delete funds;

            get_stock_portfolio(nullptr, nullptr,[](stock_portfolio*p, void* param){
                AllAssets *me = reinterpret_cast<AllAssets*>(param);
                auto* stocks = static_cast<StockPortfolio*>(p);
                me->load_stocks(stocks);
                delete stocks;
                me->cv.notify_one(); 
            },param);

        }, this);
    }

    {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk);
    }
}

// for unit tests
AllAssets::AllAssets(QuoteBySymbol&& quotes, AllBrokers *brokers, FundPortfolio* fp, StockPortfolio* sp)
{
    q = nullptr;
    quotes_by_symbol = quotes;
    
    load_cash(brokers);
    load_funds(fp);
    load_stocks(sp);
}

AllAssets::~AllAssets(){
    free_quotes(q);
}

double AllAssets::to_main_ccy(double value, const char* ccy, const char* main_ccy)
{
    if(strcmp(ccy, main_ccy) == 0){
        return value;
    }

    auto fx = quotes_by_symbol.find(std::string(ccy) + main_ccy);
    if(fx != quotes_by_symbol.end()){
        return value * fx->second->rate;
    }
    else{
        // try the other convention 
        auto fx2 = quotes_by_symbol.find(std::string(main_ccy) + ccy);
        if(fx == quotes_by_symbol.end()) return std::nan("");
        return value / fx2->second->rate;
    }
}

double AllAssets::get_price(const char* symbol)
{
    auto r = quotes_by_symbol.find(std::string(symbol));
    return r == quotes_by_symbol.end() ? std::nan("") : r->second->rate;
}

void AllAssets::load_funds(FundPortfolio* fp)
{
    for(const Fund& fund: *fp){
        items.push_back(AssetItem(ASSET_TYPE_FUNDS, fund.broker, "JPY", fund.market_value, fund.profit));
    }
}

void AllAssets::load_stocks(StockPortfolio* sp)
{
    const double nan = std::nan("");

    for(auto const& stockWithTx: *sp){
        auto* tx_list = static_cast<StockTxList*>(stockWithTx.tx_list);

        std::vector<StockTx*> sorted_tx;
        std::transform(tx_list->ptr_begin(), tx_list->ptr_end(), sorted_tx.begin(), [](StockTx* tx){return tx;});
        std::sort(sorted_tx.begin(), sorted_tx.end(), [](const StockTx* tx1, const StockTx* tx2){return strcmp(tx1->broker, tx2->broker);});

        // group tx by broker
        for(auto&& by_broker: iter::groupby(sorted_tx, [](const StockTx* tx){ return std::string(tx->broker); })){
            auto& broker = by_broker.first;
            auto balance = StockTxList::calc(by_broker.second.begin(), by_broker.second.end());
            if(balance.shares == 0) continue;
            double value, profit =  nan;
            double price = get_price(stockWithTx.instrument->symbol);
            if(!std::isnan(price)){
                value = price * balance.shares;
                profit = (price - balance.vwap) * balance.shares;
            }
            items.push_back(AssetItem(ASSET_TYPE_STOCK, broker, stockWithTx.instrument->currency, value, profit));
        }
    }
}

void AllAssets::load_cash(AllBrokers *brokers)
{
    for(Broker& broker: *brokers){
        if(broker.size(default_member_tag()) == 0){
            // no cash
            continue;
        }
        for(const CashBalance& balance: broker){
            items.push_back(AssetItem(ASSET_TYPE_CASH, broker.name, balance.ccy, balance.balance, 0));
        }
    }
}    

static std::map<asset_handle, AllAssets*> all_assets_by_handle;
static asset_handle next_asset_handle = 0;
asset_handle load_assets()
{
    asset_handle h = ++next_asset_handle;
    all_assets_by_handle[h] = new AllAssets();
    return h;
}
void free_assets(asset_handle handle)
{
    auto assets = all_assets_by_handle.find(handle);
    if(assets != all_assets_by_handle.end()){
        delete assets->second;
        all_assets_by_handle.erase(assets);
    }
}

overview* get_overview(AllAssets* assets, const char* main_ccy, GROUP level1_group, GROUP level2_group)
{
    return nullptr;
}

overview* get_overview(asset_handle asset_handle, const char* main_ccy, GROUP level1_group, GROUP level2_group)
{
    auto assets = all_assets_by_handle.find(asset_handle);
    if(assets != all_assets_by_handle.end()){
        return get_overview(assets->second, main_ccy, level1_group, level2_group);
    }
    else{
        LOG(ERROR) << "Cannot find assets by handle " << asset_handle << "\n";
        return nullptr;
    }
}

void free_overview(overview* o)
{
    delete static_cast<Overview*>(o);
}
//// Overview calculation End