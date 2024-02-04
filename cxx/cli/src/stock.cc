#include "cli.hh"
#include <core/urph-fin-core.hxx>
#include <core/stock.hxx>
#include <iostream>
#include <tabulate/table.hpp>

using namespace std;
using namespace tabulate;

namespace{

class IStockTx{
    public:
        virtual ~IStockTx(){}
        virtual void add_headers(const std::vector<std::string>& cols) = 0;
        virtual void add_row(const char* symbol, const std::string& date, const char* broker, const char* side, const std::string& price, const std::string& shares, const std::string& fee) = 0;
        virtual void print() = 0;
};

static void list_stock_tx(const char *broker, const char *symbol, IStockTx *tx)
{
    get_stock_portfolio(
        broker, symbol, [](stock_portfolio *p, void *param)
        {
            IStockTx *tx = reinterpret_cast<IStockTx*>(param);
            tx->add_headers({"Symbol","Date", "Broker", "Type", "Price", "Shares", "Fee"});
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
                tx->add_row(
                    c.first,
                    format_timestamp(c.second->date),
                    c.second->broker,
                    c.second->Side(),
                    format_with_commas(c.second->price),
                    format_with_commas(c.second->shares),
                    format_with_commas(c.second->fee)
                );
            }
            free_stock_portfolio(p);
            tx->print();
            delete tx;
        },reinterpret_cast<void*>(tx));
}

class StockTxTable : public IStockTx{
    private:
        Table table;
        ostream* out;
    public:
        StockTxTable(ostream* o):out(o){}
        virtual ~StockTxTable(){}
        virtual void add_headers(const std::vector<std::string>& cols){
            Table::Row_t cells;
            cells.reserve(cols.size());
            for(const auto& str: cols){
                cells.emplace_back(str);
            }
            table.add_row(cells);
        }
        virtual void add_row(const char* symbol, const std::string& date, const char* broker, const char* side, const std::string& price, const std::string& shares, const std::string& fee){
            table.add_row({symbol, date,broker, side,  price,  shares, fee});
        }
        virtual void print(){
            table[0].format().font_style({FontStyle::bold}).font_align(FontAlign::center);
            for(auto i = 4 ; i <= 6 ;++i) table.column(i).format().font_align(FontAlign::right);
            *out << "\n" << table << endl; 
        }
};

class StockTxCsv : public IStockTx{
    public:
        virtual ~StockTxCsv(){}
        virtual void add_headers(const std::vector<std::string>& cols){
            for(const auto& c: cols){
                std::cout<<c<<",";
            }
            std::cout<<std::endl;
        }
        virtual void add_row(const char* symbol, const std::string& date, const char* broker, const char* side, const std::string& price, const std::string& shares, const std::string& fee){
            std::cout << symbol << "," << date << "," << broker << "," << side << ",\"" << price << "\",\"" << shares << "\",\"" << fee << "\"" <<std::endl;
        }
        virtual void print(){ notify_waiting_thread(); }
};
}

void list_stock_tx(const char *broker, const char *symbol, ostream &out)
{
    StockTxTable* tx =  new StockTxTable(&out);
    list_stock_tx(broker, symbol, tx);
}

void list_stock_tx(const char *broker, const char *symbol)
{
    StockTxCsv* tx =  new StockTxCsv();
    list_stock_tx(broker, symbol, tx);
}