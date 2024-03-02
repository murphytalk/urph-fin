 #include "stock.hxx"
 #include <exception>
 #include <cstring>

#include "../utils.hxx"

Stock::Stock(const std::string_view& n, const std::string_view& ccy,asset_class_ratio&& ratios)
{
    symbol = copy_str(n);
    currency = copy_str(ccy);
    asset_class_ratios = std::move(ratios);
}

Stock& Stock::operator=(Stock&& o)
{
    free();
    symbol = o.symbol;
    currency = o.currency;
    o.symbol = nullptr;
    o.currency = nullptr;
    asset_class_ratios = std::move(o.asset_class_ratios);
    return *this;
}

void Stock::free()
{
    delete []symbol;
    delete []currency;
}

Stock::~Stock()
{
    free();
}

StockTx::StockTx(const std::string_view& b, double s, double p, double f, const std::string_view& sd, timestamp dt)
{
    broker = copy_str(b);
    shares = s;
    price = p;
    fee = f;
    side = sd == "BUY" ? BUY : ( sd == "SELL" ? SELL : SPLIT);
    date = dt;
}

StockTx::~StockTx()
{
    delete []broker;
}

StockTxList::StockTxList(int n, stock_tx *first)
{
    num = n;
    first_tx = first;
}

StockTxList& StockTxList::operator=(StockTxList&& o)
{
    free();
    num = o.num;
    first_tx = o.first_tx;
    o.num = 0;
    o.first_tx = nullptr;
    return *this;
}

StockTxList::~StockTxList()
{
    free();
}

void StockTxList::free()
{
    free_placement_allocated_structs<StockTxList, StockTx>(this);
    delete []first_tx;
}

StockWithTx::StockWithTx(stock* i, stock_tx_list* t)
{
    instrument = i;
    tx_list = t;
}

StockWithTx::~StockWithTx()
{
    //owner of the instrument pointer is stock_portfolio, leave the memory free to it
    delete static_cast<StockTxList*>(tx_list);
}

StockPortfolio::StockPortfolio(int n, stock* first_s,stock_with_tx* first)
{
    num = n;
    first_stock = first_s;
    first_stock_with_tx = first;
}

StockPortfolio::~StockPortfolio()
{
    free_placement_allocated_structs<StockPortfolio, StockWithTx>(this);
    delete []first_stock_with_tx;

    free_placement_allocated_structs<StockPortfolio, Stock>(this, stock_tag());
    delete []first_stock;
}


stock_balance StockTxList::calc()
{
    return StockTxList::calc(this->ptr_begin(), this->ptr_end());
}