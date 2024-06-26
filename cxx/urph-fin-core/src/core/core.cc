#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#include <pwd.h>
#endif  // _WIN32

#include <cstring>
#include <cassert>
#include <numeric>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <algorithm>
#include <thread>
#include <chrono>
#include <tuple>

#include "../storage/storage.hxx"
#include "urph-fin-core.hxx"
#include "stock.hxx"

#include "../utils.hxx"
#include "core_internal.hxx"

#include "../storage/storage.hxx"

#ifdef YAHOO_FINANCE
#include "../../mkt-data-src/yahoo-finance/quote.hpp"
#endif

// dont forget to free
char* copy_str(const char* str, size_t size)
{
    auto* p = new char[size + 1];
    strncpy(p, str, size);
    p[size] = 0;
    return p;
}

char* copy_str(const std::string_view& str)
{
    return copy_str(str.data(), str.size());
}

char* copy_str(const std::string& str)
{
    return copy_str(str.c_str(), str.size());
}

CashBalance::CashBalance(const std::string_view& n, float v)
{
    ccy = copy_str(n);
    balance = v;
}

CashBalance::~CashBalance()
{
    delete[] ccy;
}

Strings::Strings(int n)
{
    capacity = n;
    if(n == 0){
        strs = nullptr;
        last_str = nullptr;
        
    }
    else{
        strs = new char* [n];
        last_str = strs;
    }
}

void Strings::add(const std::string_view& i, float increment_ratio )
{
    if(size() == capacity){
        increase_capacity(std::ceil(capacity *  increment_ratio));
    }
    *last_str++ = copy_str(i);
}

void Strings::increase_capacity(long additional)
{
    if(additional <= 0 )  return;
    LDEBUG( "increase strings capacity by " << additional << " from " << capacity);

    capacity += additional;
    auto** new_strs = new char*[capacity];
    auto** new_head = new_strs;
    for(char** p=strs; p!=last_str; ++p){
        *new_strs++=*p;
    }
    delete []strs;
    strs = new_head;
    last_str = new_strs;
}

long Strings::size() const
{
    return last_str - strs;
}

char** Strings::to_str_array()
{
    auto ** ids = new char*[size()];
    char ** pp = ids;
    for(char* p: *this){
        *pp++ = p;
    }
    return ids;
}

Strings::~Strings()
{
    for(char** p=strs; p!=last_str; ++p){
        LDEBUG( *p << ",");
        delete []*p;
    }
    LDEBUG(" deleted. capacity=" << capacity << ",allocated=" <<size());
    delete []strs;
}

void free_strings(strings* ss)
{
    delete static_cast<Strings*>(ss);
}

Broker::Broker(const std::string_view&n, int ccy_num, cash_balance* first_ccy_balance, char* yyyymmdd, strings* active_funds)
{
    LDEBUG( "broker constructor: " << n);
    name = copy_str(n);
    num = ccy_num;
    first_cash_balance = first_ccy_balance;
    funds_update_date = yyyymmdd;
    active_fund_ids = active_funds;
}

Broker::~Broker(){
    LDEBUG( "freeing broker " << name << " : cash balances:");
    delete[] name;

    delete[] funds_update_date;

    free_placement_allocated_structs<Broker, CashBalance>(this);
    delete[] first_cash_balance;

    LDEBUG( " - active funds");
    free_strings(active_fund_ids);
    LDEBUG( " - active funds freed!");
}

AllBrokers::AllBrokers(int n, broker* broker)
{
    num = n;
    first_broker = broker;
}

AllBrokers::~AllBrokers()
{
    LDEBUG( "freeing " << num << " brokers");
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

Fund::Fund(const std::string_view& b,  const std::string_view&n, int a, double c, double m, double prc, double p, double r,asset_class_ratio&& ratios, timestamp d)
{
    broker = copy_str(b);
    name = copy_str(n);
    amount = a;
    capital = c;
    market_value = m;
    price = prc;
    profit = p;
    ROI = r;
    asset_class_ratios = std::move(ratios);
    date = d;
}

Fund::~Fund()
{
    delete []broker;
    delete []name;
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

OverviewItem::OverviewItem(const std::string& n, const std::string& ccy,double v, double v2, double p, double p2)
{
    name = copy_str(n);
    currency = copy_str(ccy);
    value = v;
    value_in_main_ccy = v2;
    profit = p;
    profit_in_main_ccy = p2;
}

OverviewItem::~OverviewItem()
{
    delete []name;
    delete []currency;
}


OverviewItemList::OverviewItemList(int n, overview_item* head)
{
    this->num = n;
    this->first = head;
}

OverviewItemList::~OverviewItemList()
{
    free_placement_allocated_structs<OverviewItemList, OverviewItem>(this);
}

OverviewItemContainer::OverviewItemContainer(const std::string& n, const std::string& item_name,double sum, double sum_profit,int num, overview_item* head)
{
    name = copy_str(n);
    this->item_name = copy_str(item_name);
    value_sum_in_main_ccy = sum;
    profit_sum_in_main_ccy = sum_profit;
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


OverviewItemContainerContainer::OverviewItemContainerContainer(const std::string& name, const std::string& item_name, double sum, double sum_profit, int num, overview_item_container* head)
{
    this->name = copy_str(name);
    this->item_name = copy_str(item_name);
    this->num = num;
    value_sum_in_main_ccy = sum;
    profit_sum_in_main_ccy = sum_profit;
    this->containers = head;
}
OverviewItemContainerContainer::~OverviewItemContainerContainer()
{
    delete []name;
    delete []item_name;
    free_placement_allocated_structs<OverviewItemContainerContainer, OverviewItemContainer>(this);
    delete []containers;
}


Overview::Overview(const std::string& item_name, double value_sum_in_main_ccy, double profit_sum_in_main_ccy,int num, overview_item_container_container* head)
{
    this->item_name = copy_str(item_name);
    this->value_sum_in_main_ccy = value_sum_in_main_ccy;
    this->profit_sum_in_main_ccy = profit_sum_in_main_ccy;
    this->num = num;
    this->first = head;
}
Overview::~Overview()
{
    delete []item_name;
    free_placement_allocated_structs<Overview, OverviewItemContainerContainer>(this);
    delete []first;
}

namespace{
    IDataStorage *storage = nullptr;
    BS::thread_pool* thread_pool = nullptr;
}

BS::thread_pool* get_thread_pool(){ return thread_pool; }

bool urph_fin_core_init(OnDone onInitDone, void* caller_provided_param)
{
#ifdef DEBUG_BUILD
    LOG_DEBUG << "Debug bulild";
#endif
    auto log_file0 = getenv("LOGFILE");
    auto verbose = getenv("VERBOSE");
    auto log_file = log_file0 == nullptr ? "urph-fin.log" :log_file0;

#ifdef AIX_LOG
    std::vector<AixLog::log_sink_ptr> sinks;
    auto log_lvl = verbose == nullptr ? AixLog::Severity::info : AixLog::Severity::debug;
    auto sink = std::make_shared<AixLog::SinkFile>(log_lvl, log_file);
    AixLog::Log::init({sink});
#endif

#ifdef P_LOG
    init(verbose == nullptr ? plog::info : plog::debug, log_file);
#endif
    LDEBUG( "urph-fin-core initializing");

    try{
        storage = create_cloud_instance(onInitDone, caller_provided_param);
        thread_pool = new BS::thread_pool();
        return true;
    }
    catch(const std::exception& e){
        LERROR( "Failed to init storage service: " << e.what());
        return false;
    }
}

void urph_fin_core_close()
{
    LINFO( "Freeing storage ... ");
    delete storage;
    LINFO( "Storage freed!");

    LINFO( "Shutting down thread pool ... ");
    delete thread_pool;
    LINFO( "Thread pool shutdown!");
}

#define TRY try{
#define CATCH(error_ret) }catch(const std::runtime_error& e) { LERROR( e.what(); return error_ret); }
#define CATCH_NO_RET }catch(const std::runtime_error& e) { LERROR( e.what()); }

// https://stackoverflow.com/questions/60879616/dart-flutter-getting-data-array-from-c-c-using-ffi
void get_brokers(OnAllBrokers onAllBrokers, void* param)
{
    assert(storage != nullptr);
    return storage->get_brokers(onAllBrokers, param);
}

void free_brokers(all_brokers* b)
{
    auto* brokers = static_cast<AllBrokers*>(b);
    delete brokers;
}

void get_broker(const char* name, OnBroker onBroker, void* param)
{
    assert(storage != nullptr);
    TRY
    storage->get_broker(name, onBroker, param);
    CATCH_NO_RET
}

void free_broker(broker* b)
{
    delete static_cast<Broker*>(b);
}

void get_funds(std::vector<FundsParam>& params, OnFunds onFunds, void*param,const std::function<void()>& clean_func)
{
    assert(storage != nullptr);
    TRY
    storage->get_funds(params, onFunds, param, clean_func);
    CATCH_NO_RET
}

struct get_active_funds_async_helper
{
    OnFunds onFunds;
    void* param;
    int fund_num = 0;
    char* fund_update_date;
    std::vector<Broker*> all_broker_pointers;

    get_active_funds_async_helper(OnFunds f, void *ctx):onFunds(f), param(ctx){}

    void run(const std::function<void()>& clean_func){
        std::vector<FundsParam> fund_params;//(all_broker_pointers.size());
        LDEBUG( "active fund helper running");
        const char* latest_update_date = nullptr;
        for(const auto* broker: all_broker_pointers){
            if(broker->active_fund_ids == nullptr || broker->funds_update_date == nullptr){
                LDEBUG( "broker " << broker->name << " does not have funds");
                continue;
            }
            LDEBUG( "broker " << broker->name << " has funds on " << broker->funds_update_date);
            if(latest_update_date == nullptr || strcmp(latest_update_date, broker->funds_update_date) < 0){
                latest_update_date = broker->funds_update_date;
            }
        }
        LDEBUG( "latest fund update date " << latest_update_date);
        for(auto* broker: all_broker_pointers){
            if(broker->active_fund_ids == nullptr || broker->funds_update_date == nullptr) continue;
            if(strcmp(latest_update_date, broker->funds_update_date) != 0){
                LDEBUG( "broker " << broker->name << " has no active funds on " << latest_update_date << ", last update date was " << broker->funds_update_date);
                continue;
            }
            for(auto it = broker->fund_begin(); it!= broker->fund_end(); ++it){
                LDEBUG( "Adding fund " << *it << " of " << broker->name);
                fund_params.emplace_back(broker->name, *it, broker->funds_update_date);
            }
        }
        LDEBUG( "active fund helper to run get funds");
        get_funds(fund_params, onFunds, param,[clean_func, this](){
            clean_func();
            delete this;
        });
    }
};


void do_get_active_funds_from_all_brokers(AllBrokers *brokers, get_active_funds_async_helper* h, const std::function<void()>& clean_func)
{
    // prerequisite: all brokers have their funds updated on the same day
    h->fund_update_date = brokers->begin()->funds_update_date;
    for(Broker& b: *brokers){
        LDEBUG( "broker " << b.name << " funds upd date " << b.funds_update_date);
        h->fund_num += b.size(Broker::active_fund_tag());
        h->all_broker_pointers.push_back(&b);
        LDEBUG( "added broker " << b.name << ", total fund num = " << h->fund_num);
    }
    LDEBUG( "about to run active fund helper");
    h->run(clean_func);
}

void get_active_funds_from_all_brokers(all_brokers *bks, bool free_the_brokers, OnFunds onFunds, void*param)
{
    auto *helper = new get_active_funds_async_helper(onFunds, param);
    auto *brokers = static_cast<AllBrokers*>(bks);
    do_get_active_funds_from_all_brokers(brokers, helper, [free_the_brokers, brokers](){
        if(free_the_brokers) delete brokers;
    });
}

void get_active_funds(const char* broker_name, OnFunds onFunds, void*param)
{
    TRY

    // we cannot capture anything in the lambda because of the raw-c callback function signature
    auto *helper = new get_active_funds_async_helper(onFunds, param);

    if(broker_name != nullptr){
        get_broker(broker_name, [](broker* b, void* ctx){
            auto *h = reinterpret_cast<get_active_funds_async_helper*>(ctx) ;
            auto* the_broker = static_cast<Broker*>(b);
            h->fund_num = the_broker->size(Broker::active_fund_tag());
            h->fund_update_date = the_broker->funds_update_date;
            h->all_broker_pointers.push_back(the_broker);
            h->run([the_broker](){delete the_broker;});
        }, helper);
    }else{
        get_brokers([](all_brokers* bks, void* ctx){
            auto *brokers = static_cast<AllBrokers*>(bks);
            auto *h = reinterpret_cast<get_active_funds_async_helper*>(ctx) ;
            do_get_active_funds_from_all_brokers(brokers, h, [brokers](){delete brokers;});
        }, helper);
    }

    CATCH_NO_RET
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
        return sum;
    });

    r.ROI = r.profit / r.capital;

    return r;
}

void get_known_stocks(OnStrings onStrings, void *ctx)
{
    assert(storage != nullptr);
    TRY
    storage->get_known_stocks(onStrings, ctx);
    CATCH_NO_RET
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

#ifdef YAHOO_FINANCE
namespace{
    int BACK_DAYS = 5;
    const char usdjpy[] = "USDJPY=X";
    const char cnyjpy[] = "CNYJPY=X";
    const char hkdjpy[] = "HKDJPY=X";

    void add_fx_pairs(strings* symbols)
    {
        auto* const sym = static_cast<Strings* const>(symbols);
        sym->increase_capacity(3);
        sym->add(usdjpy);
        sym->add(cnyjpy);
        sym->add(hkdjpy);
    }
}
void get_quotes(strings* symbols, OnProgress onProgress, void *progress_ctx, OnQuotes onQuotes, void* quotes_context)
{
    assert(storage != nullptr);

    using Payload = std::tuple<OnProgress,void*, OnQuotes, void*>;

    if(symbols == nullptr){
        auto* p = new Payload(onProgress, progress_ctx , onQuotes, quotes_context);
        storage->get_known_stocks([](auto* stocks, void* ctx){
           auto *payload = reinterpret_cast<Payload*>(ctx);
           add_fx_pairs(stocks);
           get_quotes(stocks, std::get<0>(*payload), std::get<1>(*payload),std::get<2>(*payload), std::get<3>(*payload));
           delete payload;
        }, p);
    }
    else{
        (void)get_thread_pool()->submit([=](){
            LDEBUG( "get quotes start");
            auto* const sym = static_cast<Strings* const>(symbols);

            auto* builder = static_cast<LatestQuotesBuilder*>(LatestQuotesBuilder::create(sym->capacity,[onQuotes, quotes_context](LatestQuotesBuilder::Alloc* alloc){
                onQuotes(new Quotes(alloc->allocated_num(), alloc->head()), quotes_context);
            }));

            int i = 0;
            for(auto name: *sym){
                LDEBUG( "Getting quote for " << name);
                onProgress(progress_ctx, ++i, sym->size());
                YahooFinance::Quote q(name);
                auto to = std::chrono::system_clock::now() - std::chrono::hours(24);
                auto from  = to - std::chrono::hours(24 * BACK_DAYS);
                q.getHistoricalSpots(
                    std::chrono::system_clock::to_time_t(from),
                    std::chrono::system_clock::to_time_t(to),
                    "1d"
                );
                auto spots = q.nbSpots();
                if(spots == 0){
                    LERROR( "No quote for " << name << " since " << BACK_DAYS << " days ago");
                }
                else{
                    auto s = q.getSpot(spots - 1);
                    builder->add_quote(name, s.getDate(), s.getClose());
                }
            }

            builder->succeed();
            delete sym;
            LDEBUG( "get quotes end");
        });
    }
}

#else

void get_quotes_async(int num, const char **symbols_head, OnQuotes onQuotes, void* caller_provided_param)
{
    assert(storage != nullptr);
    TRY
    storage->get_quotes(num, symbols_head, onQuotes, caller_provided_param);
    CATCH_NO_RET
}

namespace { std::condition_variable cv;}
quotes* get_quotes(int num, const char **symbols_head)
{
    quotes* all_quotes;
    std::mutex m;
    {
        std::lock_guard lk(m);
        try{
            get_quotes_async(num, symbols_head,[](quotes*q, void* arg){
                auto** all_quotes_ptr = reinterpret_cast<quotes**>(arg);
                *all_quotes_ptr = q;
                cv.notify_one();
            }, &all_quotes);
        }
        catch(std::runtime_error& e)
        {
            LERROR(e.what());
            all_quotes = nullptr;
            cv.notify_one();
        }
    }
    {
        std::unique_lock lk(m);
        cv.wait(lk);
    }
    return all_quotes;
}
#endif

void get_all_quotes(QuoteBySymbol& quotes_by_symbol, OnProgress OnProgress, void* progress_ctx)
{
    get_quotes(nullptr, OnProgress, progress_ctx, [](quotes* all_quotes, void* ctx){
        auto* q = reinterpret_cast<QuoteBySymbol*>(ctx);
        auto *all = static_cast<Quotes*>(all_quotes);
        for(auto const& quote: *all){
            q->add(quote.symbol, &quote);
        }
        q->notify(all_quotes);
    }, &quotes_by_symbol);
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


void update_cash_balance(const char* broker, const char* ccy, double balance, OnDone onDone, void* caller_provided_param)
{
    assert(storage != nullptr);
    TRY
    storage->update_cash(broker, ccy, balance,  onDone, caller_provided_param);
    CATCH_NO_RET
}

//// Overview calculation Start
namespace{
    const char ASSET_TYPE_STOCK [] = "Stock&ETF";
    const char ASSET_TYPE_FUNDS [] = "Funds";
    const char ASSET_TYPE_CASH  [] = "Cash";

    const char assets_tag[] = "assets";
}

AllAssets::AllAssets(const std::function<void()>& onLoaded, OnProgress onProgress, void* progress_ctx):notifyLoaded(onLoaded), quotes_by_symbol(nullptr){
    load(onProgress, progress_ctx);
}

void AllAssets::load(OnProgress onProgress, void* progress_ctx){
    quotes_by_symbol = new QuoteBySymbol([this](quotes* all_quotes){
        this->q = static_cast<::Quotes*>(all_quotes);
        this->notify(AllAssets::Loaded::Quotes);
        // stock portfolio value calculation needs quotes to be loaded first
        get_stock_portfolio(nullptr, nullptr,[](stock_portfolio*p, void* param){
            AllAssets *me = reinterpret_cast<AllAssets*>(param);
            me->stocks = static_cast<StockPortfolio*>(p);
            me->load_stocks(me->stocks);
            me->notify(AllAssets::Loaded::Stocks);
        },this);
    });

    get_brokers([](all_brokers* b, void* param){
            // if this lambda captures any closures its signature won't match the raw C function pointer declaration
            auto *me = reinterpret_cast<AllAssets*>(param);
            auto *brokers = static_cast<AllBrokers*>(b);
            me->load_cash(brokers);

            get_active_funds_from_all_brokers(brokers,true, [](fund_portfolio* fp, void *param){
                AllAssets *me = reinterpret_cast<AllAssets*>(param);
                me->funds = static_cast<FundPortfolio*>(fp);
                me->load_funds(me->funds);
                me->notify(AllAssets::Loaded::Funds);
            }, me);

            me->notify(AllAssets::Loaded::Brokers);
    }, this);

    get_all_quotes(*quotes_by_symbol, onProgress, progress_ctx);
}

void AllAssets::notify(AllAssets::Loaded loaded){
    load_status |= loaded;
    if (all_loaded(load_status)){
        notifyLoaded();
    }
}

// for unit tests
AllAssets::AllAssets(QuoteBySymbol& quotes, AllBrokers *brokers, FundPortfolio* fp, StockPortfolio* sp)
{
    q = nullptr;
    quotes_by_symbol = &quotes;

    if(brokers!=nullptr) load_cash(brokers);
    if(fp!=nullptr) load_funds(fp);
    if(sp!=nullptr) load_stocks(sp);

    funds =  fp;
    stocks = sp;
}

AllAssets::~AllAssets(){
    delete funds;
    delete stocks;
    delete q;
}


std::set<std::string> AllAssets::get_all_ccy() const
{
    std::set<std::string> all_ccy;
    for(auto& i: this->items){
        all_ccy.insert(i.currency);
    }
    return all_ccy;
}


// stock and FX pairs have quotes, so if we exclude all stocks from quotes what's left is FX pairs
std::set<std::string> AllAssets::get_all_ccy_pairs() const
{
    std::set<std::string> all_ccy_pairs;
    for (auto& [symbol, _]: quotes_by_symbol->mapping) {
        all_ccy_pairs.insert(symbol);
    }
    for(auto& stx: *stocks){
        all_ccy_pairs.erase(std::string(stx.instrument->symbol));
    }
    return all_ccy_pairs;
}

double AllAssets::to_main_ccy(double value, const char* ccy, const char* main_ccy)
{
    if(strcmp(ccy, main_ccy) == 0){
        return value;
    }

    auto fx = quotes_by_symbol->mapping.find(std::string(ccy) + main_ccy + "=X");
    if(fx != quotes_by_symbol->mapping.end()){
        return value * fx->second->rate;
    }
    else{
        // try the other convention
        auto fx2 = quotes_by_symbol->mapping.find(std::string(main_ccy) + ccy + "=X");
        if(fx == quotes_by_symbol->mapping.end()) return std::nan("");
        return value / fx2->second->rate;
    }
}

double AllAssets::get_price(const char* symbol) const
{
    auto r = quotes_by_symbol->mapping.find(std::string(symbol));
    return r == quotes_by_symbol->mapping.end() ? std::nan("") : r->second->rate;
}

template<typename RandomAccessIterator,  typename FieldSelectorUnaryFn>
auto group_by(RandomAccessIterator _first, RandomAccessIterator _last, const FieldSelectorUnaryFn& fieldChooser)
{
    using FieldType = decltype(fieldChooser(*_first));
    std::map<FieldType, std::vector<typename RandomAccessIterator::value_type>> instancesByField;
    for(RandomAccessIterator i = _first; i != _last; ++i)
    {
        instancesByField[fieldChooser(*i)].push_back(*i);
    }
    return instancesByField;
}

void AllAssets::load_funds(FundPortfolio* fp)
{
    using ValueProfit = std::pair<double, double>;
    // don't iterate by Fund& , as group_by would copy it to vector and it will trigger ~Fund() , causing double free when the original Fund frees string it manages
    for(auto& by_broker: group_by(fp->ptr_begin(), fp->ptr_end(),[](Fund* f){ return std::string(f->broker); })){
        auto const& broker = by_broker.first;
        auto vp = std::accumulate(by_broker.second.begin(), by_broker.second.end(), std::make_pair(0.0, 0.0), [](ValueProfit& a, Fund* x){
            a.first += x->market_value;
            a.second += x->profit;
            return a;
        });
        items.emplace_back(ASSET_TYPE_FUNDS, broker.c_str(), "JPY", vp.first, vp.second);
    }
}

void AllAssets::load_stocks(StockPortfolio* sp)
{
    const double nan = std::nan("");

    AssetItems grouped_by_sym_and_broker;
    for(auto const& stockWithTx: *sp){
        StockTxList *tx_list = static_cast<StockTxList*>(stockWithTx.tx_list);
        LDEBUG( "stock=" << stockWithTx.instrument->symbol);
        // group tx by broker
        for(auto& by_broker: group_by(tx_list->ptr_begin(),tx_list->ptr_end(), [](const StockTx* tx){ return std::string(tx->broker); })){
            auto& broker = by_broker.first;
            LDEBUG( "broker=" << broker);
            const auto& balance = StockTxList::calc(by_broker.second.begin(), by_broker.second.end());
            if(balance.shares == 0) continue;
            double value, profit =  nan;
            double price = get_price(stockWithTx.instrument->symbol);
            if(!std::isnan(price)){
                value = price * balance.shares;
                profit = (price - balance.vwap) * balance.shares;
            }
            grouped_by_sym_and_broker.emplace_back(ASSET_TYPE_STOCK, const_cast<std::string&>(broker), stockWithTx.instrument->currency, value, profit);
        }
    }
    // merge items with same broker and ccy
    for(auto& by_broker_ccy: group_by(grouped_by_sym_and_broker.begin(), grouped_by_sym_and_broker.end(), [](const AssetItem& i) -> std::string { return i.broker + i.currency; })){
        auto& first = by_broker_ccy.second.front();
        auto& list = by_broker_ccy.second;
        items.push_back(std::accumulate(list.begin(), list.end(), AssetItem(first.asset_type, first.broker.c_str(), first.currency.c_str(),0.0,0.0),
            [](AssetItem& a, AssetItem& x){
                a.profit += x.profit;
                a.value  += x.value;
                return a;
            }
        ));
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

namespace{
    std::map<AssetHandle, AllAssets*> all_assets_by_handle;
    AssetHandle next_asset_handle = 0;
    AllAssets* get_assets_by_handle(AssetHandle asset_handle)
    {
        auto assets = all_assets_by_handle.find(asset_handle);
        if(assets != all_assets_by_handle.end()){
            return assets->second;
        }
        else{
            LERROR( "Cannot find assets by handle " << asset_handle);
            return nullptr;
        }
    }
}

void load_assets(OnAssetLoaded onLoaded, void* ctx,OnProgress onProgress, void* progressCtx)
{
    AssetHandle h = ++next_asset_handle;
    all_assets_by_handle[h] = new AllAssets([onLoaded=std::move(onLoaded), h, ctx, onProgress=std::move(onProgress), progressCtx]{ onLoaded(ctx, h); }, onProgress, progressCtx);
}

const Quote* AllAssets::get_latest_quote(const char* symbol) const
{
    auto it = quotes_by_symbol->mapping.find(std::string(symbol));
    return it == quotes_by_symbol->mapping.end() ? nullptr : it->second;
}


const quote* get_latest_quote(AllAssets* assets, const char* symbol)
{
    return assets == nullptr ? nullptr : assets->get_latest_quote(symbol);
}

const quote* get_latest_quote(AssetHandle handle, const char* symbol)
{
    return get_latest_quote(get_assets_by_handle(handle), symbol);
}


const quotes* get_latest_quotes(AllAssets* assets, int num, const char** symbols)
{
    if(assets == nullptr) return nullptr;

    Quotes *quotes = nullptr;
    auto *builder = static_cast<LatestQuotesBuilder *>(LatestQuotesBuilder::create(num, [&quotes](LatestQuotesBuilder::Alloc *alloc){
        quotes = new Quotes(alloc->allocated_num(), alloc->head());
    }));

    for(int i = 0; i < num; ++i){
        char* p = const_cast<char*>(symbols[i]);
        auto *q = assets->get_latest_quote(p);
        if(q!=nullptr){
            builder->add_quote(p, q->date, q->rate);
        }
    }
    builder->succeed();

    return quotes;
}
const quotes* get_latest_quotes(AssetHandle handle, int num, const char** symbols)
{
    return get_latest_quotes(get_assets_by_handle(handle), num, symbols);
}

const quotes* get_latest_quotes_delimeter(AllAssets* assets, int num, const char* delimiter, const char* symbols)
{
    char* input = new char[strlen(symbols) + 1];
    strcpy(input, symbols);

#ifdef _MSC_VER
    const char** p = new const char*[num];
#else
    const char* p[num];
#endif
    auto cur = strtok(input, delimiter);
    int idx = 0;
    while(cur != nullptr){
        p[idx++] = cur;
        cur = strtok(nullptr, delimiter);
    }

    auto *q = get_latest_quotes(assets, num, p);

    delete []input;
#ifdef _MSC_VER
    delete []p;
#endif

    return q;
}

const quotes* get_latest_quotes_delimeter(AssetHandle handle, int num, const char* delimiter , const char* symbols)
{
    return get_latest_quotes_delimeter(get_assets_by_handle(handle), num, delimiter, symbols);
}

strings* get_all_ccy(AllAssets* assets)
{
    const auto all_ccy = assets->get_all_ccy();
    StringsBuilder sb(all_ccy.size());
    for(auto i = all_ccy.cbegin(); i != all_ccy.cend(); ++i){
        sb.add(*i);
    }
    return sb.strings;
}

quotes* get_all_ccy_pairs_quote(AllAssets* assets)
{
    Quotes *quotes = nullptr;

    auto all_pairs = assets->get_all_ccy_pairs();

    auto *builder = static_cast<LatestQuotesBuilder *>(LatestQuotesBuilder::create(all_pairs.size(), [&quotes](LatestQuotesBuilder::Alloc *alloc){
        quotes = new Quotes(alloc->allocated_num(), alloc->head());
    }));

    for(auto& pair: all_pairs) {
        auto *q = get_latest_quote(assets, pair.c_str());
        if(q!=nullptr){
            builder->add_quote(pair.c_str(), q->date, q->rate);
        }
    }
    builder->succeed();
    return quotes;
}

const quotes* get_all_ccy_pairs_quote(AssetHandle handle)
{
    return get_all_ccy_pairs_quote(get_assets_by_handle(handle));
}

strings* get_all_ccy(AssetHandle handle)
{
    return get_all_ccy(get_assets_by_handle(handle));
}

void free_assets(AssetHandle handle)
{
    auto assets = all_assets_by_handle.find(handle);
    if(assets != all_assets_by_handle.end()){
        delete assets->second;
        all_assets_by_handle.erase(assets);
    }
}

namespace{
    char GROUP_ASSET [] = "Asset";
    char GROUP_BROKER[] = "Broker";
    char GROUP_CCY   [] = "Currency";
    char *GROUPS[] = {GROUP_ASSET, GROUP_BROKER, GROUP_CCY};
    char overview_tag[] = "overview";
}

struct LvlGroup
{
    LvlGroup(GROUP byGroup){
        switch (byGroup)
        {
        case GROUP_BY_ASSET:
            key = [](const AssetItem& a){ return std::string(a.asset_type);};
            group_name = GROUP_ASSET;
            break;
        case GROUP_BY_BROKER:
            key = [](const AssetItem& a){ return a.broker;};
            group_name = GROUP_BROKER;
            break;
        case GROUP_BY_CCY:
            key = [](const AssetItem& a){ return a.currency;};
            group_name = GROUP_CCY;
            break;
        default:
            group_name = nullptr;
            key = nullptr;
            break;
        }
    }
    std::string group_name;
    std::function<std::string(const AssetItem&)> key;
    inline std::string operator() (const AssetItem& a) const { return key(a); }
};

overview* get_overview(AllAssets* assets, const char* main_ccy, GROUP level1_group, GROUP level2_group, GROUP level3_group)
{
    auto lvl1 = LvlGroup(level1_group);
    auto lvl2 = LvlGroup(level2_group);
    auto lvl3 = LvlGroup(level3_group);

    LDEBUG( "lvl1=" << lvl1.group_name << ",lvl2="<<lvl2.group_name<<",lvl3="<<lvl3.group_name);

    double lvl1_sum = 0.0, lvl1_sum_profit = 0.0;
    const auto& lvl1_grp = group_by(assets->items.begin(),assets->items.end(), lvl1);
    PlacementNew<overview_item_container_container> container_container_alloc(lvl1_grp.size());
    for(auto& l1: lvl1_grp){
        const auto& l1_name = l1.first;
        auto& lvl2_grp = l1.second;

        LDEBUG( "Level 1 " << l1_name);

        PlacementNew<overview_item_container> container_alloc(lvl2_grp.size());

        double lvl2_sum = 0.0, lvl2_sum_profit = 0.0;
        for(auto& l2: group_by(lvl2_grp.begin(),lvl2_grp.end(),lvl2)){
            const auto& l2_name = l2.first;
            LDEBUG( "Level 2 " << l2_name);
            PlacementNew<overview_item> item_alloc(l2.second.size());
            double sum = 0.0, sum_profit = 0.0;
            for(auto&& l3: l2.second){
                double main_ccy_value  = assets->to_main_ccy(l3.value, l3.currency.c_str(), main_ccy);
                double main_ccy_profit = assets->to_main_ccy(l3.profit,l3.currency.c_str(), main_ccy);
                new (item_alloc.next()) OverviewItem(lvl3.key(l3),l3.currency, l3.value, main_ccy_value,l3.profit, main_ccy_profit);
                sum += main_ccy_value;
                sum_profit += main_ccy_profit;
            }
            new (container_alloc.next()) OverviewItemContainer(l2_name, lvl3.group_name, sum, sum_profit, item_alloc.allocated_num(), item_alloc.head());
            lvl2_sum += sum;
            lvl2_sum_profit += sum_profit;
        }

        new (container_container_alloc.next()) OverviewItemContainerContainer(l1_name, lvl2.group_name, lvl2_sum, lvl2_sum_profit, container_alloc.allocated_num(), container_alloc.head());
        lvl1_sum += lvl2_sum;
        lvl1_sum_profit += lvl2_sum_profit;
    }

    return new Overview(lvl1.group_name, lvl1_sum, lvl1_sum_profit, container_container_alloc.allocated_num(), container_container_alloc.head());
}

overview* get_overview(AssetHandle asset_handle, const char* main_ccy, GROUP level1_group, GROUP level2_group, GROUP level3_group)
{
    return get_overview(get_assets_by_handle(asset_handle), main_ccy, level1_group, level2_group, level3_group);
}

overview_item_list* get_sum_group(AllAssets* assets, const char* main_ccy, GROUP group)
{
    if(assets == nullptr) return nullptr;

    const LvlGroup lvl(group);
    const auto& groupedData = group_by(assets->items.begin(),assets->items.end(), lvl);
    PlacementNew<overview_item> item_alloc(groupedData.size());
    typedef std::pair<double, double> ValueAndProfit;
    std::string dummyCcy;
    for(auto& data : groupedData){
        ValueAndProfit s = {0.0, 0.0};
        for(const auto&a : data.second){
            s.first += assets->to_main_ccy(a.value, a.currency.c_str(), main_ccy);
            s.second+= assets->to_main_ccy(a.profit, a.currency.c_str(), main_ccy);
        };
        new (item_alloc.next()) OverviewItem(data.first, dummyCcy, 0.0, s.first, 0.0, s.second);
    }
    return new OverviewItemList(item_alloc.allocated_num(), item_alloc.head());
}

overview_item_list* get_sum_group_by_asset(AssetHandle asset_handle, const char* main_ccy)
{
    return get_sum_group(get_assets_by_handle(asset_handle), main_ccy, GROUP_BY_ASSET);
}

overview_item_list* get_sum_group_by_broker(AssetHandle asset_handle, const char* main_ccy)
{
    return get_sum_group(get_assets_by_handle(asset_handle), main_ccy, GROUP_BY_BROKER);
}

void free_overview_item_list(overview_item_list *list)
{
    delete static_cast<OverviewItemList*>(list);
}

void free_overview(overview* o)
{
    delete static_cast<Overview*>(o);
}
//// Overview calculation End