#include "cli.hh"
#include <cstring>
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

void list_stock_tx(const char *broker, const char *symbol, IStockTx *tx)
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
            str_vect_to_table_row(table,cols);
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

class IStockPos{
    public:
        virtual ~IStockPos(){}
        virtual void add_row(stock_balance& balance,const StockWithTx& stockWithTx) = 0;
        virtual void print() = 0;
};

class StockPosCsv : public IStockPos{
    public:
        StockPosCsv(){
            std::cout<<"Symbol,Currency,VWAP,Shares,Liquidated,Fee" << std::endl;
        }
        virtual void add_row(stock_balance& balance,const StockWithTx& stockWithTx){
            std::cout<< stockWithTx.instrument->symbol << "," << stockWithTx.instrument->currency << ","  << balance.vwap << ","  << balance.shares << ","  << balance.liquidated << ","  << balance.fee << std::endl;
        }
        virtual void print(){ notify_waiting_thread(); }
};



const char jpy[] = "JPY";
class StockPosTable : public IStockPos{
    private:
        Table table;
        ostream* out;
        int row;
        double market_value_sum_jpy;
        double profit_sum_jpy;
        timestamp fx_date;
        std::function<std::pair<double, timestamp>(const std::string &symbol)> get_rate;
    public:
        StockPosTable(ostream* o,std::function<std::pair<double, timestamp>(const std::string &symbol)> r):out(o),row(0),market_value_sum_jpy(0.0),profit_sum_jpy(0.0),fx_date(0L),get_rate(r){
            table.add_row({"Symbol", "Currency", "VWAP", "Price", "Shares", "Market Value", "Market Value(JPY)", "Profit", "Profit(JPY)", "Liquidated", "Liquidated(JPY)", "Fee", "Date"});
        }
        virtual void add_row(stock_balance& balance,const StockWithTx& stockWithTx){
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
                           format_with_commas(fx_rate * market_value),
                           format_with_commas(profit),
                           format_with_commas(profit_jpy),
                           format_with_commas(balance.liquidated),
                           format_with_commas(fx_rate * balance.liquidated),
                           format_with_commas(balance.fee),
                           format_timestamp(quote_date)});
            if (!std::isnan(profit) && profit < 0)
            {
                table[row][7].format().font_color(Color::red);
                table[row][8].format().font_color(Color::red);
            }
        }
        virtual void print(){
            ++row;
            table.add_row({"SUM", "", "", "", "", "", format_with_commas(market_value_sum_jpy), "", format_with_commas(profit_sum_jpy), "", "", "FX Date", format_timestamp(fx_date)});
            if (profit_sum_jpy < 0)
                table[row][8].format().font_color(Color::red);
            table[row].format().font_style({FontStyle::bold}).font_align(FontAlign::right);
            for (auto i = 2; i <= 11; ++i){
                table.column(i).format().font_align(FontAlign::right);
            }
            table[0].format().font_style({FontStyle::bold}).font_align(FontAlign::center);
            *out << "\n"
                 << table << endl;
        }
};

void list_stock_pos(const char *symbol, IStockPos *pos)
{
    get_stock_portfolio(
        nullptr, symbol, [](stock_portfolio *p, void *param)
        {
            auto* pos = reinterpret_cast<IStockPos*>(param);
            auto *port = static_cast<StockPortfolio*>(p);
            for (const auto &stockWithTx : *port)
            {
                auto *tx_list = static_cast<StockTxList*>(stockWithTx.tx_list);
                auto balance = tx_list->calc();
                if (balance.shares == 0 || std::isnan(balance.shares))
                   continue;
                pos->add_row(balance, stockWithTx);
            }
            pos->print();
            delete pos;
            delete p;
        },
        pos
    ); 
}

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

void list_stock_pos(const char *symbol, std::ostream &out,std::function<std::pair<double, timestamp>(const std::string &symbol)> get_rate)
{
    auto *pos = new StockPosTable(&out, get_rate);
    list_stock_pos(symbol, pos);
}

void list_stock_pos()
{
    auto *pos = new StockPosCsv();
    list_stock_pos(nullptr, pos);
}