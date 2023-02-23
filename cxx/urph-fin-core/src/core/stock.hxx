#ifndef URPH_FIN_STOCK_HXX_
#define URPH_FIN_STOCK_HXX_

#include "urph-fin-core.hxx"
#include "../utils.hxx"
#include <deque>
#include <cmath>
#include <execution>
#include <numeric>

namespace{
    const char log_tag[] = "urph-fin-storage";
}

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
    Iterator<StockTx> end()   { return Iterator( head(default_member_tag()) + num ); }

    PtrIterator<StockTx*> ptr_begin() { return PtrIterator( head(default_member_tag()) ); }
    PtrIterator<StockTx*> ptr_end()   { return PtrIterator( head(default_member_tag()) + num ); }


    stock_balance calc();

    template<typename _RandomAccessIterator>
    static stock_balance calc(_RandomAccessIterator _first, _RandomAccessIterator _last){
        auto unclosed_positions = std::deque<UnclosedPosition>();
        stock_balance balance = {0.0, 0.0, 0.0, 0.0};

        for(_RandomAccessIterator i = _first; i != _last; ++i){
            StockTx* tx = *i;
            balance.fee += tx->fee;
            if(tx->side == SPLIT){
                // 1 to N share split, here price is the N
                balance.shares *= tx->price;
                balance.shares = floor(balance.shares);
                // apply to all unclosed positions
                for(auto& c: unclosed_positions){
                    c.split(tx->price);
                }
            }
            else{
                auto s = tx->side  == BUY ? tx->shares : (tx->shares > 0 ? -1 : 1) * tx->shares;
                balance.shares += s;
                balance.liquidated -= tx->price * s;
    
                if(s>0){
                    // buy
                    unclosed_positions.push_back({tx->price, tx->shares, tx->fee});
                }
                else{
                    // sell
                    auto first_unclosed_pos = unclosed_positions.front().shares;
                    for(auto shares = tx->shares; shares > 0;){
                        if(unclosed_positions.empty()){
                            const double& n = std::nan("");
                            return {n, n, n, n};                        }
                        else if (first_unclosed_pos > shares){
                            unclosed_positions.begin()->shares = first_unclosed_pos - shares;
                            shares = 0;
                        }
                        else if (first_unclosed_pos == shares){
                            unclosed_positions.pop_front();
                            shares = 0;
                        }
                        else{
                            shares -= first_unclosed_pos;
                            unclosed_positions.pop_front();
                            first_unclosed_pos = unclosed_positions.front().shares;
                        }
                    }
                }
            }
        }
        auto r = std::accumulate(unclosed_positions.begin(), unclosed_positions.end(), UnclosedPosition{0.0, 0.0, 0.0},
            [](UnclosedPosition& a, UnclosedPosition& x){
                a.fee += x.fee;
                // price here is used to save market value
                a.price += x.price * x.shares;
                a.shares += x.shares;
                return a;
            }
        );
        balance.vwap = r.shares == 0 ? 0 : r.price  / r.shares;
        
        return balance;
    }
private:
    void free();
    class UnclosedPosition{
    public:
        double price;
        double shares;
        double fee;
    
        void split(double factor){
            price  /=  factor;
            shares *=  factor;
        }
    };
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