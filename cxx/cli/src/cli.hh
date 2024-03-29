#ifndef URPH_FIN_CLI_HXX_
#define URPH_FIN_CLI_HXX_

#include <string>
#include <sstream>
#include <locale>
#include <cmath>
#include <iostream>
#include <vector>

#include <tabulate/table.hpp>
#include <core/urph-fin-core.hxx>

template <typename T>
std::string format_with_commas(T value)
{
    if (std::isnan(value))
        return "N/A";

    std::stringstream ss;
    ss.imbue(std::locale(""));
    ss << std::fixed << value;
    return ss.str();
}

template <typename T>
std::string percentage(T value)
{
    std::stringstream ss;
    ss << value * 100 << "%";
    return ss.str();
}

std::string format_timestamp(timestamp t);

void notify_waiting_thread();
void wait_for_notification();
void str_vect_to_table_row(tabulate::Table& table,const std::vector<std::string>& cols);

void list_stock_tx(const char *broker, const char *symbol, std::ostream &out);
void list_stock_tx(const char *broker, const char *symbol);

void list_funds(std::string& broker_name, std::ostream &out);
void list_funds(std::string broker_name);

void list_stock_pos(const char *symbol, std::ostream &out,std::function<std::pair<double, timestamp>(const std::string &symbol)> get_rate);
void list_stock_pos();

void list_broker(std::ostream &out);
void list_broker();

#endif

