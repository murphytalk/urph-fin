#include <iostream>
#include <memory>
#include <string>
#include <mutex>
#include <condition_variable>

#include <urph-fin-core.hxx>
#include "cli/clilocalsession.h"
#include <cli/loopscheduler.h>
#include <cli/cli.h>
#include <tabulate/table.hpp>

using namespace std;
using namespace tabulate;

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
                    bool broker_name_printed = false;
                    for(CashBalance& balance: broker){
                        if (broker_name_printed)
                            table.add_row(
                                {"", balance.ccy, to_string(balance.balance)}
                            );
                        else{
                            table.add_row(
                                {broker.name, balance.ccy, to_string(balance.balance)}
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
                out << table << endl;
            },
            "List broker summary"
        );

        rootMenu->Insert(
            "fund",
            [](ostream& out, string broker){
                get_funds(broker.c_str(), [](fund_portfolio* fp){
                    Table table;
                    table.add_row({"Broker", "Fund Name", "Amount", "Price", "Capital", "Market Value", "Profit"});
                    auto fund_portfolio = static_cast<FundPortfolio*>(fp);
                    for(Fund& fund: *fund_portfolio){
                        table.add_row({fund.broker, fund.name, to_string(fund.amount), to_string(fund.price), to_string(fund.capital), to_string(fund.market_value), to_string(fund.market_value - fund.capital)});
                    }
                    free_funds(fp);
                });
            },
            "List mutual funds portfolio by broker"
        );

        rootMenu->Insert(
            "stock",
            [](ostream& out, string broker){
                out<<"Show " << broker << " Stock & ETF portfolio\n";
            },
            "List stock and ETF portfolio by broker"
        );


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
