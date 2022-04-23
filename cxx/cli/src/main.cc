#include <iostream>
#include <memory>
#include <mutex>
#include <condition_variable>

#include <urph-fin-core.hxx>
#include "cli/clilocalsession.h"
#include <cli/loopscheduler.h>
#include <cli/cli.h>

using namespace std;


void main_menu()
{
    try
    {
        auto rootMenu = make_unique< cli::Menu >( "urph-fin" );

        size_t brokers_num;
        char **all_broker_names = get_all_broker_names(&brokers_num);

        rootMenu->Insert(
            "brokers",
            [](ostream& out){
                auto brokers = static_cast<AllBrokers*>(get_brokers());
                out << brokers->to_str() << "\n";
                free_brokers(brokers);
            },
            "List broker summary"
        );

        auto portfolioMenu = make_unique<cli::Menu>( "funds" );
        auto portfolioMenuCallback = [](std::ostream& out, std::string broker){
            out<<"selected " << broker << "\n";
        };
        char** broker = all_broker_names;
        for(size_t i = 0; i < brokers_num ; ++i, ++broker){
            portfolioMenu->Insert(*broker,portfolioMenuCallback, string("List funds summary in broker ") + *broker);
        }
        rootMenu -> Insert(std::move(portfolioMenu));

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

        free_broker_names(all_broker_names, brokers_num);

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
