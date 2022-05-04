#include <iostream>
#include <memory>
#include <string>
#include <mutex>
#include <iomanip>
#include <locale>
#include <condition_variable>

#include <core/urph-fin-core.hxx>
#include <core/stock.hxx>

#include "cli/clilocalsession.h"
#include <cli/loopscheduler.h>
#include <cli/cli.h>
#include <tabulate/table.hpp>

using namespace std;
using namespace tabulate;

template<typename T> std::string format_with_commas(T value)
{
    std::stringstream ss;
    ss.imbue(std::locale(""));
    ss << std::fixed << value;
    return ss.str();
}

template<typename T> std::string percentage(T value)
{
    std::stringstream ss;
    ss << value * 100 << "%";
    return ss.str();
}

static void main_menu()
{
    try
    {
        auto rootMenu = make_unique< cli::Menu >( "urph-fin" );

        size_t brokers_num;
        char **all_broker_names = get_all_broker_names(&brokers_num);

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
                    for(CashBalance& balance: broker){
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
                    for(Fund& fund: *fund_portfolio){
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
            "List mutual funds portfolio by broker"
        );


        auto stockMenu = make_unique<cli::Menu >( "stock" );
        stockMenu->Insert(
            "list",
            [](ostream& out){
                auto stocks = get_known_stocks(nullptr);
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

        rootMenu->Insert(std::move(stockMenu));


        cli::Cli cli( std::move(rootMenu) );
        // global exit action
        cli.ExitAction( [](auto& out){ out << "Goodbye.\n"; } );

        cli::LoopScheduler scheduler;
        cli::CliLocalTerminalSession localSession(cli, scheduler, std::cout, 200);
        localSession.ExitAction(
            [&scheduler](auto& out) // session exit action
            {
                scheduler.Stop();
            }
        );


        scheduler.Run();

        free_broker_names(all_broker_names, brokers_num);

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
    if(!urph_fin_core_init([](){
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
