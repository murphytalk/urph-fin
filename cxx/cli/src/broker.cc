#include "cli.hh"
#include <core/urph-fin-core.hxx>
#include <iostream>
#include <iomanip> // Required for std::fixed

using namespace std;
using namespace tabulate;

namespace{

class IBroker{
    public:
        virtual ~IBroker(){}
        virtual void add_headers(const std::vector<std::string>& cols) = 0;
        virtual void add_row(char* name,char* ccy , double value) = 0;
        virtual void print() = 0;
};

class BrokerTable : public IBroker{
    private:
        Table table;
        ostream& out;
        string last_name;
    public:
        BrokerTable(ostream& o):out(o),last_name(""){}
        virtual void add_headers(const std::vector<std::string>& cols){
            str_vect_to_table_row(table,cols);
        }
        virtual void add_row(char* name,char* ccy , double value){
            if(ccy == nullptr){
                table.add_row({name, "", ""});
            }
            else{
                table.add_row(
                    { last_name == name ? "": name, ccy, format_with_commas(value)}
                );
                last_name = name;
            }
        }
        virtual void print(){
            table.column(1).format().font_align(FontAlign::center);
            table.column(2).format().font_align(FontAlign::right);
            table[0].format()
                .font_style({FontStyle::bold})
                .font_align(FontAlign::center);
            out << "\n" << table << endl; 
        }
};

class BrokerCsv : public IBroker{
    public:
        virtual void add_headers(const std::vector<std::string>& cols){
            for(const auto& c: cols){
                std::cout<<c<<",";
            }
            std::cout<<std::endl;
        }
        virtual void add_row(char* name,char* ccy , double value){
            if(ccy == nullptr || value == 0) return;
            std::cout<<name<<","<<ccy<<","<<std::fixed<<value<<std::endl;
        }
        virtual void print(){ notify_waiting_thread(); }
};

void list(IBroker* l)
{
    get_brokers([](auto *bks, void *ctx){
        auto* l = reinterpret_cast<IBroker*>(ctx);
        auto* brokers = static_cast<AllBrokers*>(bks);

        l->add_headers({"Broker", "Currency", "Balance"});
        for(Broker& broker: *brokers){
            if(broker.size(default_member_tag()) == 0){
                l->add_row(broker.name, nullptr, 0);
                continue;
            }

            for(const CashBalance& balance: broker){
                l->add_row(broker.name, balance.ccy, balance.balance);
            }
        }
        l->print();
        delete brokers;
       },l);
}


}


void list_broker(std::ostream &out){
    list(new BrokerTable(out));
}

void list_broker(){
    list(new BrokerCsv());
}