#include <iostream>
#include <mutex>
#include <condition_variable>

#include <urph-fin-core.h>
#include "cli/clilocalsession.h"
#include <cli/loopscheduler.h>
#include <cli/cli.h>

using namespace std;

void main_menu()
{
    try
    {
        auto rootMenu = make_unique< cli::Menu >( "urph-fin" );

        rootMenu->Insert(
            "brokers",
            [](ostream& out){
                out<<"brokers ..,"<<endl;
            },
            "List broker summary"
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
            std::cout << "init done" << std::endl;
            auto brokers = get_brokers();
            free_brokers(brokers);
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
