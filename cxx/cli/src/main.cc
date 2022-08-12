#include <iostream>
#include <memory>
#include <string>
#include <mutex>
#include <iomanip>
#include <locale>
#include <condition_variable>
#include <cmath>
#include <ctime>
#include <unordered_map>


#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <core/urph-fin-core.hxx>
#include <core/stock.hxx>

#include "cli/clilocalsession.h"
#include <cli/loopscheduler.h>
#include <cli/cli.h>
#include <tabulate/table.hpp>

using namespace std;
using namespace tabulate;

static std::string format_timestamp(timestamp t)
{
    using namespace boost::posix_time;
    using namespace boost::gregorian;
    ptime pt = from_time_t(t);
    return to_simple_string(pt);
}

template<typename T> static std::string format_with_commas(T value)
{
    if(std::isnan(value)) return "N/A";

    std::stringstream ss;
    ss.imbue(std::locale(""));
    ss << std::fixed << value;
    return ss.str();
}

template<typename T> static std::string percentage(T value)
{
    std::stringstream ss;
    ss << value * 100 << "%";
    return ss.str();
}

static quotes* all_quotes = nullptr;
static std::unordered_map<std::string, const Quote*> quotes_by_symbol;
static void get_all_quotes()
{
    if(all_quotes == nullptr){
        //todo: the async version is conflicting with get stock portfolio ...
        all_quotes = get_quotes(0, nullptr);
        auto* q = static_cast<Quotes*>(all_quotes);
        for(auto const& quote: *q){
            quotes_by_symbol[quote.symbol] = &quote;
        }
    }
}


static const char jpy[] = "JPY";
static inline double to_jpy(double fx_rate, double value)
{
    return std::isnan(fx_rate) || std::isnan(value) ? std::nan("") : fx_rate * value;
}
static double get_rate(const std::string& symbol)
{
    auto it = quotes_by_symbol.find(symbol);
    return it == quotes_by_symbol.end() ? std::nan("") : it->second->rate;
}

static void print_stock_list(ostream& out, stock_portfolio*p)
{
    Table table;
    table.add_row({"Symbol", "Currency", "VWAP", "Price", "Shares", "Market Value", "Market Value(JPY)", "Profit", "Profit(JPY)", "Liquidated", "Liquidated(JPY)", "Fee"});
    auto *port = static_cast<StockPortfolio*>(p);
    double market_value_sum_jpy, profit_sum_jpy = 0.0;
    int row = 0;
    for(auto const& stockWithTx: *port){
        auto* tx_list = static_cast<StockTxList*>(stockWithTx.tx_list);
        auto balance = tx_list->calc();
        if(balance.shares == 0) continue;
        ++row;
        double fx_rate = 1.0;
        if(strncmp(jpy, stockWithTx.instrument->currency, sizeof(jpy)/sizeof(char)) != 0){
            fx_rate = get_rate(stockWithTx.instrument->currency + std::string(jpy));
        }
        double market_value, profit,profit_jpy = std::nan("");
        double price = get_rate(stockWithTx.instrument->symbol);
        if(!std::isnan(price) && !std::isnan(fx_rate)){
            market_value =  price * balance.shares;
            profit = (price - balance.vwap) * balance.shares;
            profit_jpy = fx_rate * profit;
            market_value_sum_jpy += fx_rate * market_value;
            profit_sum_jpy += profit_jpy;
        }
        table.add_row({
            stockWithTx.instrument->symbol,
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
            format_with_commas(balance.fee)
        });
        if(!std::isnan(profit) && profit < 0){
            table[row][7].format().font_color(Color::red);
            table[row][8].format().font_color(Color::red);
        }
    }
    ++row;
    table.add_row({"SUM", "", "", "", "", "", format_with_commas(market_value_sum_jpy), "", format_with_commas(profit_sum_jpy), "", "", ""});
    if(profit_sum_jpy<0) table[row][8].format().font_color(Color::red);
    table[row].format().font_style({FontStyle::bold}).font_align(FontAlign::right);
    free_stock_portfolio(p);
    for(auto i = 2 ; i <= 11 ;++i) table.column(i).format().font_align(FontAlign::right);
    table[0].format().font_style({FontStyle::bold}).font_align(FontAlign::center);
    out << "\n" << table << endl;
}

static void list_stock_tx(const char* broker,const char* symbol, ostream& out)
{
    get_stock_portfolio(broker, symbol,[](stock_portfolio*p, void* param){
        ostream* out = reinterpret_cast<ostream*>(param);
        Table table;
        table.add_row({"Symbol","Date", "Broker", "Type", "Price", "Shares", "Fee"});
        auto *port = static_cast<StockPortfolio*>(p);
        std::vector<std::pair<const char*,StockTx*>> all_tx;
        for(StockWithTx& stx: *port){
            auto *tx_list = static_cast<StockTxList*>(stx.tx_list);
            for(StockTx& tx: *tx_list){
                all_tx.push_back(std::make_pair(stx.instrument->symbol,&tx));
           }
        }
        std::sort(all_tx.begin(), all_tx.end(),[](const auto& c1, const auto& c2){ return c1.second->date > c2.second->date; });
        for(const auto&c: all_tx){
            table.add_row({
                c.first,
                format_timestamp(c.second->date),
                c.second->broker,
                c.second->Side(),
                format_with_commas(c.second->price),
                format_with_commas(c.second->shares),
                format_with_commas(c.second->fee)
            });
        }
        free_stock_portfolio(p);
        table[0].format().font_style({FontStyle::bold}).font_align(FontAlign::center);
        for(auto i = 4 ; i <= 6 ;++i) table.column(i).format().font_align(FontAlign::right);
        *out << "\n" << table << endl;
    }, &out);
}

static void main_menu()
{
    try
    {
        auto rootMenu = make_unique< cli::Menu >( "home" );

        rootMenu->Insert(
            "brokers",
            [](ostream& out){
                Table table;
                table.add_row({"Broker", "Currency", "Balance"});
                auto brokers = static_cast<AllBrokers*>(get_brokers());
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
                free_brokers(brokers);
                table.column(1).format().font_align(FontAlign::center);
                table.column(2).format().font_align(FontAlign::right);
                table[0].format()
                    .font_style({FontStyle::bold})
                    .font_align(FontAlign::center);
                out << "\n" << table << endl;
            },
            "List broker summary"
        );

        rootMenu->Insert(
            "fund",
            [](ostream& out, string broker_name){
                get_active_funds( broker_name == "all" ? nullptr : broker_name.c_str(),[](fund_portfolio* fp, void *param){
                    ostream* out = reinterpret_cast<ostream*>(param);
                    Table table;
                    table.add_row({"Broker", "Fund Name", "Amount", "Price", "Capital", "Market Value", "Profit", "ROI"});
                    auto fund_portfolio = static_cast<FundPortfolio*>(fp);
                    int row = 0;
                    for(const Fund& fund: *fund_portfolio){
                        ++row;
                        table.add_row({fund.broker, fund.name, 
                            format_with_commas(fund.amount), format_with_commas(fund.price), format_with_commas(fund.capital), format_with_commas(fund.market_value), 
                            format_with_commas(fund.profit), percentage(fund.ROI)});
                        if(fund.profit < 0){
                            table[row][6].format().font_color(Color::red);
                            table[row][7].format().font_color(Color::red);
                        }
                    }

                    ++row;
                    auto sum = calc_fund_sum(fund_portfolio);
                    table.add_row({"SUM", "", "", "", format_with_commas(sum.capital), format_with_commas(sum.market_value), format_with_commas(sum.profit), percentage(sum.ROI)});
                    table[row].format().font_style({FontStyle::bold}).font_align(FontAlign::right);
                    if(sum.profit < 0){
                        table[row][6].format().font_color(Color::red);
                        table[row][7].format().font_color(Color::red);
                    }

                    free_funds(fp);

                    for(auto i = 2 ; i <= 7 ;++i) table.column(i).format().font_align(FontAlign::right);
                    table.column(1).format().multi_byte_characters(true);
                    table[0].format().font_style({FontStyle::bold}).font_align(FontAlign::center);
                    *out << "\n" << table << endl;
                }, &out);

            },
            "List mutual funds portfolio by broker, all use all for all funds"
        );


        auto stockMenu = make_unique<cli::Menu >( "stock" );
        stockMenu->Insert(
            "known",
            [](ostream& out){
                auto stocks = get_known_stocks();
                Table table;
                table.add_row({"Symbol"});
                table[0].format().font_style({FontStyle::bold}).font_align(FontAlign::center);
                Strings* stock_names = static_cast<Strings*>(stocks);
                for(char* n: *stock_names){
                    table.add_row({n});
                }
                free_strings(stocks);
                out << "\n" << table << endl;
            },
            "List all known stocks"
        );
        stockMenu->Insert(
            "list",
            [](ostream& out){
                get_all_quotes();
                get_stock_portfolio(nullptr, nullptr,[](stock_portfolio*p, void* param){
                    ostream* out = reinterpret_cast<ostream*>(param);
                    print_stock_list(*out, p);
               }, &out);
            },
            "List stock balance"
        );
        stockMenu->Insert(
            "sym",
            [](ostream& out, string symbol){
                get_all_quotes();
                get_stock_portfolio(nullptr, symbol.c_str(),[](stock_portfolio*p, void* param){
                    ostream* out = reinterpret_cast<ostream*>(param);
                    print_stock_list(*out, p);
               }, &out);
            },
            "List specified stock's balance"
        );
        stockMenu->Insert(
            "tx",
            [](ostream& out, string symbol){
                list_stock_tx(nullptr, symbol == "all" ? nullptr : symbol.c_str(), out);
            },
            "List specified stock's transactions, or use all for all stocks"
        );
        stockMenu->Insert(
            "btx",
            [](ostream& out, string broker){
                list_stock_tx(broker.c_str(), nullptr, out);
            },
            "List specified broker's transactions"
        );
        stockMenu->Insert(
            "bstx",
            [](ostream& out, string broker, string symbol){
                list_stock_tx(broker.c_str(), symbol.c_str(), out);
            },
            "List transactions by broker and symbol"
        );
        stockMenu->Insert(
            "add",
            [](ostream& out, string broker, string symbol, double shares, double price, double fee,string side, string yyyy_mm_dd){
                using namespace boost::gregorian;
                using namespace boost::posix_time;
                using namespace boost;

                to_upper(side);
                if(side != "BUY" && side != "SELL" && side != "SPLIT"){
                    out << "Unknown side " << side << "\n";
                    return;
                }
                // date
                date d(from_simple_string(yyyy_mm_dd));
                auto t = to_time_t(ptime(d));

                out << "Adding broker=" << broker << " symbol=" << symbol
                    << " shares=" << shares << " price=" << price << " side=" << side << " date=" << d << "\n";
                add_stock_tx(broker.c_str(), symbol.c_str(), shares, price, fee, side.c_str(), t, [](void *param){ 
                    ostream* out = reinterpret_cast<ostream*>(param);
                    *out << "Added\n";
                }, &out);
            },
            "Add new transaction : broker symbol shares price fee side date(yyyy-mm-dd)"
        );
 

        rootMenu->Insert(std::move(stockMenu));


        cli::Cli cli( std::move(rootMenu) );
        // global exit action
        cli.ExitAction( [](auto& out){ 
            out << "Goodbye.\n"; 
        });

        cli::LoopScheduler scheduler;
        cli::CliLocalTerminalSession localSession(cli, scheduler, std::cout, 200);
        localSession.ExitAction(
            [&scheduler](auto& out) // session exit action
            {
                free_quotes(all_quotes);
                scheduler.Stop();
            }
        );


        scheduler.Run();

    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}

static bool init_done = false;
static std::mutex m;
static std::condition_variable cv;

int main(int argc, char *argv[])
{
    if(!urph_fin_core_init([](void*){
        {
            std::lock_guard<std::mutex> lk(m);
            init_done = true;
        }
        cv.notify_one();
    })){
        cout<<"Failed to init";
        return 1;
    }

    {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk, []{return init_done;});
    }

    main_menu(); 
   
    urph_fin_core_close();
    return 0;
}
