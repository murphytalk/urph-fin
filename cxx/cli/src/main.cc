#define URPH_FIN_CLI

#include <iostream>
#include <memory>
#include <string>
#include <mutex>
#include <iomanip>
#include <sstream>
#include <locale>
#include <condition_variable>
#include <ctime>
#include <tuple> 

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <core/urph-fin-core.hxx>
#include <core/stock.hxx>

#include "cli/clilocalsession.h"
#include <cli/loopscheduler.h>
#include <cli/cli.h>
#include <tabulate/table.hpp>
#include <cxxopts.hpp>

#include <indicators/block_progress_bar.hpp>
#include <indicators/cursor_control.hpp>

#include "cli.hh"

using namespace std;
using namespace tabulate;

namespace{
    bool wait_flag = false;
    std::mutex m;
    std::condition_variable cv;
}

void notify_waiting_thread()
{
    std::lock_guard<std::mutex> lk(m);
    wait_flag = true;
    cv.notify_one(); 
}

void wait_for_notification()
{
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, []{ return wait_flag; });
}

std::string format_timestamp(timestamp t)
{
    // Convert Unix timestamp to std::time_t
    std::time_t time = static_cast<std::time_t>(t);

    // Convert to local time
    std::tm* localTime = std::localtime(&time);

    // Create a string stream
    std::ostringstream oss;

    // Format the time and put it in the string stream
    oss << std::put_time(localTime, "%Y-%m-%d %H:%M:%S");

    // Return the formatted string
    return oss.str();    
}

void str_vect_to_table_row(Table& table,const std::vector<std::string>& cols)
{
    Table::Row_t cells;
     cells.reserve(cols.size());
     for(const auto& str: cols){
        cells.emplace_back(str);
     }
     table.add_row(cells);
}
 

namespace{

QuoteBySymbol *quotes_by_symbol = nullptr;
quotes *all_quotes = nullptr;

void print_progress(void *ctx, int current, int all)
{
    auto *p = reinterpret_cast<indicators::BlockProgressBar*>(ctx);
    float percentage = 100.0 * current / all;
    //std::cout << "..." << percentage << std::flush;
    p->set_progress(percentage);
}

indicators::BlockProgressBar* prepare_progress_bar()
{
    return new indicators::BlockProgressBar {
        indicators::option::BarWidth{80},
        indicators::option::Start{"["},
        indicators::option::End{"]"},
        indicators::option::ShowElapsedTime{true},
        indicators::option::PostfixText{"Loading quotes"},
        indicators::option::ForegroundColor{indicators::Color::yellow}  ,
        indicators::option::FontStyles{std::vector<indicators::FontStyle>{indicators::FontStyle::bold}}
    };            
}

void get_quotes(std::function<void()> onQuotesLoaded)
{
    if (all_quotes == nullptr)
    {
        auto* bar = prepare_progress_bar();
        // cannot capture the callback function obj by reference as it will be executed in another thread after the passed in obj goes out of scope
        quotes_by_symbol = new QuoteBySymbol([onQuotesLoaded = std::move(onQuotesLoaded),bar](quotes *aq){
            delete bar;
            all_quotes = aq;
            onQuotesLoaded(); 
        });

       get_all_quotes(*quotes_by_symbol, print_progress, bar);
    }
    else
        onQuotesLoaded();
}

const char jpy[] = "JPY";
inline double to_jpy(double fx_rate, double value)
{
    return std::isnan(fx_rate) || std::isnan(value) ? std::nan("") : fx_rate * value;
}
std::pair<double, timestamp> get_rate(const std::string &symbol)
{
    auto it = quotes_by_symbol->mapping.find(symbol);
    return it == quotes_by_symbol->mapping.end() ? std::make_pair(std::nan(""), (timestamp)0L) : std::make_pair(it->second->rate, it->second->date);
}

void print_stock_list(ostream &out, stock_portfolio *p)
{
    Table table;
    table.add_row({"Symbol", "Currency", "VWAP", "Price", "Shares", "Market Value", "Market Value(JPY)", "Profit", "Profit(JPY)", "Liquidated", "Liquidated(JPY)", "Fee", "Date"});
    auto *port = static_cast<StockPortfolio *>(p);
    double market_value_sum_jpy = 0.0, profit_sum_jpy = 0.0;
    int row = 0;

    timestamp fx_date = 0L;
    for (auto const &stockWithTx : *port)
    {
        auto *tx_list = static_cast<StockTxList *>(stockWithTx.tx_list);
        //LOG_DEBUG("cli", "calc position of " << stockWithTx.instrument->symbol);
        auto balance = tx_list->calc();
        if (balance.shares == 0 || std::isnan(balance.shares))
            continue;
        ++row;
        double fx_rate = 1.0;
        if (strncmp(jpy, stockWithTx.instrument->currency, sizeof(jpy) / sizeof(char)) != 0)
        {
            const auto &r = get_rate(stockWithTx.instrument->currency + std::string(jpy) + "=X");
            fx_rate = r.first;
            fx_date = r.second;
        }
        double market_value = std::nan("");
        double profit = std::nan("");
        double profit_jpy = std::nan("");
        const auto &r = get_rate(stockWithTx.instrument->symbol);
        double price = r.first;
        timestamp quote_date = r.second;
        if (!std::isnan(price) && !std::isnan(fx_rate))
        {
            market_value = price * balance.shares;
            profit = (price - balance.vwap) * balance.shares;
            profit_jpy = fx_rate * profit;
            market_value_sum_jpy += fx_rate * market_value;
            profit_sum_jpy += profit_jpy;
        }
        else
        {
            //            LOG_ERROR("cli", "no quote found for " << stockWithTx.instrument->symbol);
        }
        table.add_row({stockWithTx.instrument->symbol,
                       stockWithTx.instrument->currency,
                       format_with_commas(balance.vwap),
                       format_with_commas(price),
                       format_with_commas(balance.shares),
                       format_with_commas(market_value),
                       format_with_commas(to_jpy(fx_rate, market_value)),
                       format_with_commas(profit),
                       format_with_commas(profit_jpy),
                       format_with_commas(balance.liquidated),
                       format_with_commas(to_jpy(fx_rate, balance.liquidated)),
                       format_with_commas(balance.fee),
                       format_timestamp(quote_date)});
        if (!std::isnan(profit) && profit < 0)
        {
            table[row][7].format().font_color(Color::red);
            table[row][8].format().font_color(Color::red);
        }
    }
    ++row;
    table.add_row({"SUM", "", "", "", "", "", format_with_commas(market_value_sum_jpy), "", format_with_commas(profit_sum_jpy), "", "", "FX Date", format_timestamp(fx_date)});
    if (profit_sum_jpy < 0)
        table[row][8].format().font_color(Color::red);
    table[row].format().font_style({FontStyle::bold}).font_align(FontAlign::right);
    free_stock_portfolio(p);
    for (auto i = 2; i <= 11; ++i)
        table.column(i).format().font_align(FontAlign::right);
    table[0].format().font_style({FontStyle::bold}).font_align(FontAlign::center);
    out << "\n"
        << table << endl;
}




const char *groupName[] = {
    "Asset", "Broker", "Currency"};

std::string main_ccy = "JPY";
AssetHandle ah = 0;

void list_overview(GROUP lvl1, GROUP lvl2, GROUP lvl3, ostream &out)
{
    Table table;

    std::ostringstream mkt_value_main_ccy;
    mkt_value_main_ccy << "Market Value (" << main_ccy << ")";
    std::ostringstream profit_main_ccy;
    profit_main_ccy << "Profit (" << main_ccy << ")";

    table.add_row({groupName[lvl1], groupName[lvl2], groupName[lvl3], "Market Value", mkt_value_main_ccy.str(), "Profit", profit_main_ccy.str()});

    auto *o = static_cast<Overview *>(get_overview(ah, main_ccy.c_str(), lvl1, lvl2, lvl3));

    int row = 0;
    for (auto &l1 : *o)
    {
        ++row;
        table.add_row({l1.name, "", "", "", format_with_commas(l1.value_sum_in_main_ccy), "", format_with_commas(l1.profit_sum_in_main_ccy)});
        if (l1.profit_sum_in_main_ccy < 0)
        {
            table[row][6].format().font_color(Color::red);
        }
        for (auto &l2 : l1)
        {
            if (l2.value_sum_in_main_ccy == 0)
                continue;
            ++row;
            table.add_row({"", l2.name, "", "", format_with_commas(l2.value_sum_in_main_ccy), "", format_with_commas(l2.profit_sum_in_main_ccy)});
            if (l2.profit_sum_in_main_ccy < 0)
            {
                table[row][6].format().font_color(Color::red);
            }
            for (auto &l3 : l2)
            {
                if (l3.value_in_main_ccy == 0)
                    continue;
                ++row;
                table.add_row({"", "", l3.name, format_with_commas(l3.value), format_with_commas(l3.value_in_main_ccy), format_with_commas(l3.profit), format_with_commas(l3.profit_in_main_ccy)});
                if (l3.profit < 0)
                {
                    table[row][5].format().font_color(Color::red);
                    table[row][6].format().font_color(Color::red);
                }
            }
        }
    }
    table.add_row({"SUM", "", "", "", format_with_commas(o->value_sum_in_main_ccy), "", format_with_commas(o->profit_sum_in_main_ccy)});
    ++row;
    table[row].format().font_style({FontStyle::bold}).font_align(FontAlign::right);

    table[0].format().font_style({FontStyle::bold}).font_align(FontAlign::center);
    for (auto i = 3; i <= 6; ++i)
        table.column(i).format().font_align(FontAlign::right);
    out << "\n"
        << table << endl;
    delete o;
}

GROUP to_lvl_group(std::string &lvl)
{
    switch (lvl[0])
    {
    case 'a':
    case 'A':
        return GROUP_BY_ASSET;
    case 'b':
    case 'B':
        return GROUP_BY_BROKER;
    default:
        return GROUP_BY_CCY;
    }
}

void main_menu()
{
    try
    {
        auto rootMenu = make_unique<cli::Menu>("home");

        auto overViewMenu = make_unique<cli::Menu>("ov");

        overViewMenu->Insert(
            "ls",
            [](ostream &out)
            {
                if (ah == 0)
                {
                    auto *bar = prepare_progress_bar();
                    auto *ctx = new std::tuple<indicators::BlockProgressBar*, ostream*>(bar, &out);
                    load_assets([](void *p, AssetHandle h){
                        auto* ctx1 = reinterpret_cast<std::tuple<indicators::BlockProgressBar*, ostream*>*>(p);
                        delete std::get<0>(*ctx1);
                        ah = h;
                        std::cout<<"asset handle " << h << std::endl;
                        list_overview(GROUP_BY_ASSET, GROUP_BY_BROKER, GROUP_BY_CCY, *std::get<1>(*ctx1)); 
                    },ctx, print_progress, bar);
                }
                else
                    list_overview(GROUP_BY_ASSET, GROUP_BY_BROKER, GROUP_BY_CCY, out);
            },
            "List by Asset-Broker-Currency => shortcut for: cs a b c");

        overViewMenu->Insert(
            "cs",
            [](ostream &out, std::string lvl1, std::string lvl2, std::string lvl3)
            {
                list_overview(to_lvl_group(lvl1), to_lvl_group(lvl2), to_lvl_group(lvl3), out);
            },
            "custom list by lv1-lvl2-lvl3, a=>Asset b=>Broker c=>Currency");

        rootMenu->Insert(std::move(overViewMenu));

        rootMenu->Insert(
            "bk",
            [](ostream &os)
            {
                get_brokers([](auto *bks, void *ctx){
                    auto* out = reinterpret_cast<ostream*>(ctx);
                    auto* brokers = static_cast<AllBrokers*>(bks);

                    Table table;
                    table.add_row({"Broker", "Currency", "Balance"});

                    for(Broker& broker: *brokers){
                        if(broker.size(default_member_tag()) == 0){
                            table.add_row({broker.name, "", ""});
                            continue;
                        }
                        bool broker_name_printed = false;
                        for(const CashBalance& balance: broker){
                            if (broker_name_printed)
                                table.add_row(
                                    {"", balance.ccy, format_with_commas(balance.balance)}
                                );
                            else{
                                table.add_row(
                                    {broker.name, balance.ccy, format_with_commas(balance.balance)}
                                );
                                broker_name_printed = true;
                            }
                        }
                    }
                    delete brokers;
                    table.column(1).format().font_align(FontAlign::center);
                    table.column(2).format().font_align(FontAlign::right);
                    table[0].format()
                        .font_style({FontStyle::bold})
                        .font_align(FontAlign::center);
                    *out << "\n" << table << endl; },
                            &os);
            },
            "List broker summary");

        rootMenu->Insert(
            "cash",
            [](ostream& out, string broker, string ccy, double balance){
                update_cash_balance(broker.c_str(), ccy.c_str(), balance, [](void *param){
                    ostream* out = reinterpret_cast<ostream*>(param);
                    *out<< "cash balance updated\n";
                }, &out);    
            },
            "Update cash balance: broker currency balance");

        rootMenu->Insert(
            "fund",
            [](ostream &out, string broker_name){ list_funds(broker_name,out); },
            "List mutual funds portfolio by broker, all use all for all funds");

        auto stockMenu = make_unique<cli::Menu>("stock");
        stockMenu->Insert(
            "known",
            [](ostream &o)
            {
                get_known_stocks([](auto *stocks, void *ctx)
                                 {
                    auto* out = reinterpret_cast<ostream*>(ctx);
                    Table table;
                    table.add_row({"Symbol"});
                    table[0].format().font_style({FontStyle::bold}).font_align(FontAlign::center);
                    auto* stock_names = static_cast<Strings*>(stocks);
                    for(char* n: *stock_names){
                        table.add_row({n});
                    }
                    free_strings(stocks);
                    *out << "\n" << table << endl; },
                                 &o);
            },
            "List all known stocks");
        stockMenu->Insert(
            "ls",
            [](ostream &out)
            {
                get_quotes([&out]()
                           { get_stock_portfolio(
                                 nullptr, nullptr, [](stock_portfolio *p, void *param)
                                 {
                                    auto* o = reinterpret_cast<ostream*>(param);
                                    print_stock_list(*o, p); 
                                 },
                                 &out); 
                            }
                );
            },
            "List stock balance");
        stockMenu->Insert(
            "sym",
            [](ostream &out, string const &symbol)
            {
                get_quotes([&out, symbol]()
                           { get_stock_portfolio(
                                 nullptr, symbol.c_str(),
                                 [](stock_portfolio *p, void *param)
                                 {
                                    auto* o = reinterpret_cast<ostream*>(param);
                                    print_stock_list(*o, p); },
                                 &out); });
            },
            "List specified stock's balance");
        stockMenu->Insert(
            "tx",
            [](ostream &out, string symbol)
            {
                list_stock_tx(nullptr, symbol == "all" ? nullptr : symbol.c_str(), out);
            },
            "List specified stock's transactions, or use all for all stocks");
        stockMenu->Insert(
            "btx",
            [](ostream &out, string broker)
            {
                list_stock_tx(broker.c_str(), nullptr, out);
            },
            "List specified broker's transactions");
        stockMenu->Insert(
            "bstx",
            [](ostream &out, string broker, string symbol)
            {
                list_stock_tx(broker.c_str(), symbol.c_str(), out);
            },
            "List transactions by broker and symbol");
        stockMenu->Insert(
            "add",
            [](ostream &out, string broker, string symbol, double shares, double price, double fee, string side, string yyyy_mm_dd)
            {
                using namespace boost::gregorian;
                using namespace boost::posix_time;
                using namespace boost;

                to_upper(side);
                if (side != "BUY" && side != "SELL" && side != "SPLIT")
                {
                    out << "Unknown side " << side << "\n";
                    return;
                }
                // date
                date d(from_simple_string(yyyy_mm_dd));
                auto t = to_time_t(ptime(d));

                out << "Adding broker=" << broker << " symbol=" << symbol
                    << " shares=" << shares << " price=" << price << " side=" << side << " date=" << d << "\n";
                add_stock_tx(
                    broker.c_str(), symbol.c_str(), shares, price, fee, side.c_str(), t, [](void *param)
                    {
                    ostream* out = reinterpret_cast<ostream*>(param);
                    *out << "Added\n"; },
                    &out);
            },
            "Add new transaction : broker symbol shares price fee side date(yyyy-mm-dd)");

        rootMenu->Insert(std::move(stockMenu));

        cli::Cli cli(std::move(rootMenu));
        // global exit action
        cli.ExitAction([](auto &out)
                       { out << "Goodbye.\n"; });

        cli::LoopScheduler scheduler;
        cli::CliLocalTerminalSession localSession(cli, scheduler, std::cout, 200);
        localSession.ExitAction(
            [&scheduler](auto &out) // session exit action
            {
                free_quotes(all_quotes);
                if (ah > 0)
                    free_assets(ah);
                scheduler.Stop();
            });

        scheduler.Run();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
}

}

int main(int argc, char *argv[])
{
    cxxopts::Options options("urph-fin-cli", "The urph-fin console app");
    options.add_options()
        ("x,tx","Trade history", cxxopts::value<bool>()->default_value("false"))
        ("r,stock","Stock/ETF position", cxxopts::value<bool>()->default_value("false"))
        ("f,fund","Funds position", cxxopts::value<bool>()->default_value("false"))
        ("h,help", "Print usage")
    ;

    auto result = options.parse(argc, argv);

    if (result.count("help"))
    {
      std::cout << options.help() << std::endl;
      exit(0);
    }

    if (!urph_fin_core_init([](void *){ notify_waiting_thread();},nullptr)){
        cout << "Failed to init";
        return 1;
    }

    // wait for init is done
    wait_for_notification();

    wait_flag = false;

    if(result["tx"].as<bool>()){
        list_stock_tx(nullptr, nullptr);
    }
    if(result["stock"].as<bool>()){
        list_stock_pos(nullptr, nullptr);
    }
    else if (result["fund"].as<bool>()){
        list_funds(std::string("all"));
    }
    else{
        main_menu();
        notify_waiting_thread();
    }

    // wait for job is done
    wait_for_notification();

    urph_fin_core_close();
    return 0;
}
