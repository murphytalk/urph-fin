#ifndef URPH_FIN_STOCK_HXX_
#define URPH_FIN_STOCK_HXX_

#include "urph-fin-core.hxx"

class Stock: public stock{
public:
    Stock(const std::string& b, const std::string& n, const std::string& ccy);
    ~Stock();
};

class StockTx: public stock_tx{
public: 
    StockTx(double s, double p, double f, const std::string side);
    const char* Side(){
        return side == BUY ? "BUY" :  (side == SELL ? "SELL" : "SPLIT");
    }
};


class StockPortfolio: public stock_portfolio{
public:
   StockPortfolio(int n, stock_tx *first);
   ~StockPortfolio();

   inline StockTx* head(default_member_tag) { return static_cast<StockTx*>(first_tx); }
   inline int size(default_member_tag) { return num; }

   Iterator<StockTx> begin() { return Iterator( head(default_member_tag()) ); }
   Iterator<StockTx> end() { return Iterator( head(default_member_tag()) + num ); }
   stock_portfolio_balance calc();
};

static_assert(sizeof(Stock)  == sizeof(stock));
static_assert(sizeof(StockTx)  == sizeof(stock_tx));
static_assert(sizeof(StockPortfolio)  == sizeof(stock_portfolio));

#endif