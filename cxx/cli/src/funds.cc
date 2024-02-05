#include "cli.hh"
#include <core/urph-fin-core.hxx>
#include <iostream>
#include <tabulate/table.hpp>

using namespace std;
using namespace tabulate;

namespace{

class IFund{
    public:
        virtual ~IFund(){}
        virtual void add_headers(const std::vector<std::string>& cols) = 0;
        virtual void add_row(const string& broker, const char* fund_name, const std::string& amt, const std::string& price, const std::string& capital, const std::string& mkt_value, double profit, const std::string& roi, const std::string& date) = 0;
        virtual void add_sum_row(const fund_sum&) = 0;
        virtual void print() = 0;
};

void list_funds(const string& broker_name, IFund* list)
{
    get_active_funds(
        broker_name == "all" ? nullptr : broker_name.c_str(), 
        [](fund_portfolio *fp, void *param)
        {
            IFund *list = reinterpret_cast<IFund*>(param);
            list->add_headers({"Broker", "Fund Name", "Amount", "Price", "Capital", "Market Value", "Profit", "ROI", "Date"});
            auto fund_portfolio = static_cast<FundPortfolio*>(fp);
            for(const Fund& fund: *fund_portfolio){
                list->add_row(
                    fund.broker, 
                    fund.name,
                    format_with_commas(fund.amount), 
                    format_with_commas(fund.price), 
                    format_with_commas(fund.capital), 
                    format_with_commas(fund.market_value),
                    fund.profit, 
                    percentage(fund.ROI), 
                    format_timestamp(fund.date)
                );
            }

            auto sum = calc_fund_sum(fund_portfolio);
            list->add_sum_row(sum);
            free_funds(fp);
            list->print();
            delete list;
        },
        reinterpret_cast<void*>(list)
    );
}

class FundTable: public IFund{
    private:
        Table table;
        ostream* out;
        int row;
    public:
        FundTable(ostream* o):out(o),row(0){}
        virtual ~FundTable(){}
        virtual void add_headers(const std::vector<std::string>& cols){
            str_vect_to_table_row(table,cols);
        }
        virtual void add_row(const string& broker, const char* fund_name, const std::string& amt, const std::string& price, const std::string& capital, const std::string& mkt_value, double profit, const std::string& roi, const std::string& date)
        {
           ++row;
           table.add_row({
                broker, fund_name,
                amt, price, capital, mkt_value,
                format_with_commas(profit), roi, date
            });
            if(profit < 0){
                table[row][6].format().font_color(Color::red);
                table[row][7].format().font_color(Color::red);
            }
        }
        virtual void add_sum_row(const fund_sum& sum)
        {
            ++row;
            table.add_row({"SUM", "", "", "", format_with_commas(sum.capital), format_with_commas(sum.market_value), format_with_commas(sum.profit), percentage(sum.ROI),""});
            table[row].format().font_style({FontStyle::bold}).font_align(FontAlign::right);
            if(sum.profit < 0){
                table[row][6].format().font_color(Color::red);
                table[row][7].format().font_color(Color::red);
            }
        }
        virtual void print()
        {
            for(auto i = 2 ; i <= 7 ;++i) table.column(i).format().font_align(FontAlign::right);
            table.column(1).format().multi_byte_characters(true);
            table[0].format().font_style({FontStyle::bold}).font_align(FontAlign::center);
            *out << "\n" << table << endl;
        }
};

class FundCsv: public IFund{
    public:    
        virtual ~FundCsv(){}
        virtual void add_headers(const std::vector<std::string>& cols){
            for(const auto& c: cols){
                std::cout<<c<<",";
            }
            std::cout<<std::endl;
        }
        virtual void add_row(const string& broker, const char* fund_name, const std::string& amt, const std::string& price, const std::string& capital, const std::string& mkt_value, double profit, const std::string& roi, const std::string& date)
        {
            std::cout << broker << "," << fund_name << ",\"" << amt << "\",\""<< price  << "\" ,\""<<  capital << "\",\""<< mkt_value << "\",\""<<  profit << "\"," << roi << "," <<  date <<std::endl;
        }
        virtual void add_sum_row(const fund_sum& sum){}
        virtual void print(){ notify_waiting_thread(); }
};
}

void list_funds(string& broker_name, ostream &out)
{
    list_funds(broker_name, new FundTable(&out));
}


void list_funds(string broker_name)
{
    list_funds(broker_name, new FundCsv());
}