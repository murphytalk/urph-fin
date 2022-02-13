#include <iostream>
#include <urph-fin-core.h>
#include "cli/clilocalsession.h"
#include <cli/loopscheduler.h>
#include <cli/cli.h>

using namespace std;

int main(int argc, char *argv[])
{
    if(!urph_fin_core_init()){
        cout<<"Failed to init";
        return 1;
    }

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
                out << "Closing App...\n";
                scheduler.Stop();
            }
        );

        scheduler.Run();

    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
    
    urph_fin_core_close();
    return 0;
}
