#ifndef URPH_FIN_CLI_HXX_
#define URPH_FIN_CLI_HXX_

#include <string>
#include <sstream>
#include <locale>
#include <cmath>
#include <iostream>

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

void list_stock_tx(const char *broker, const char *symbol, std::ostream &out);
void list_stock_tx(const char *broker, const char *symbol);

#endif

