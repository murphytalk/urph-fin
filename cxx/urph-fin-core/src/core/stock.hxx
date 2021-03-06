#ifndef URPH_FIN_STOCK_HXX_
#define URPH_FIN_STOCK_HXX_

#include "urph-fin-core.hxx"

class Stock: public stock{
public:
    Stock(){
        symbol = nullptr;
        currency = nullptr;
    }
    Stock(const std::string& n, const std::string& ccy);
    Stock& operator=(Stock&&);
    ~Stock();
private:
    void free();
};

class StockTx: public stock_tx{
public: 
    StockTx(const std::string& b, double s, double p, double f, const std::string side,timestamp );
    ~StockTx();
    const char* Side() const{
        return side == BUY ? "BUY" :  (side == SELL ? "SELL" : "SPLIT");
    }
};

class StockTxList: public stock_tx_list{
public:
    StockTxList(int n, stock_tx *first);
    StockTxList& operator=(StockTxList&&);
    ~StockTxList();

    inline StockTx* head(default_member_tag) { return static_cast<StockTx*>(first_tx); }
    inline int size(default_member_tag) { return num; }

    Iterator<StockTx> begin() { return Iterator( head(default_member_tag()) ); }
    Iterator<StockTx> end() { return Iterator( head(default_member_tag()) + num ); }
    stock_balance calc();
private:
    void free();
};


class StockWithTx: public stock_with_tx{
public:
    StockWithTx(stock* i, stock_tx_list* t);
    ~StockWithTx();
};

class StockPortfolio: public stock_portfolio{
public:
    StockPortfolio(int n, stock* first_stock, stock_with_tx* first);
    ~StockPortfolio();

    inline StockWithTx* head(default_member_tag) { return static_cast<StockWithTx*>(first_stock_with_tx); }
    inline int size(default_member_tag) { return num; }
    Iterator<StockWithTx> begin() { return Iterator( head(default_member_tag()) ); }
    Iterator<StockWithTx> end() { return Iterator( head(default_member_tag()) + num ); }

    struct stock_tag {};
    inline Stock* head(stock_tag) { return static_cast<Stock*>(first_stock); }
    inline int size(stock_tag) { return num; }
    Iterator<Stock> stock_begin() { return Iterator( head(stock_tag()) ); }
    Iterator<Stock> stock_end() { return Iterator( head(stock_tag()) + num ); }
 };

static_assert(sizeof(Stock)  == sizeof(stock));
static_assert(sizeof(StockTx)  == sizeof(stock_tx));
static_assert(sizeof(StockTxList)  == sizeof(stock_tx_list));
static_assert(sizeof(StockWithTx)  == sizeof(stock_with_tx));
static_assert(sizeof(StockPortfolio)  == sizeof(stock_portfolio));

#endif