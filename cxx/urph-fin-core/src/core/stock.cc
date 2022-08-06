 #include "stock.hxx"
 #include <exception>
 #include <deque>
 #include <numeric>
 #include <execution>
 #include <cstring>
 #include <cmath>

#include "../utils.hxx"

Stock::Stock(const std::string& n, const std::string& ccy)
{
    symbol = copy_str(n);
    currency = copy_str(ccy);
}

Stock& Stock::operator=(Stock&& o)
{
    free();
    symbol = o.symbol;
    currency = o.currency;
    o.symbol = nullptr;
    o.currency = nullptr;
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

StockTx::StockTx(const std::string& b, double s, double p, double f, const std::string sd, timestamp dt)
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

static const double NaN = std::nan("");
stock_balance StockTxList::calc()
{
    auto unclosed_positions = std::deque<UnclosedPosition>();
    stock_balance balance = {0.0, 0.0, 0.0, 0.0};

    for(const auto& tx: *this){
        balance.fee += tx.fee;
        if(tx.side == SPLIT){
            // 1 to N share split, here price is the N
            balance.shares *= tx.price;
            // apply to all unclosed positions
            for(auto& c: unclosed_positions){
                c.split(tx.price);
            }
        }
        else{
            auto s = tx.side  == BUY ? tx.shares : (tx.shares > 0 ? -1 : 1) * tx.shares;
            balance.shares += s;
            balance.liquidated -= tx.price * s;

            if(s>0){
                // buy
                unclosed_positions.push_back({tx.price, tx.shares, tx.fee});
            }
            else{
                // sell
                auto first_unclosed_pos = unclosed_positions.front().shares;
                for(auto shares = tx.shares; shares > 0;){
                    if(unclosed_positions.empty()){
                        LOG(ERROR) << "Not enough unclosed position: still have " << shares << " shares to sell";
                        return {NaN, NaN, NaN, NaN};
                    }
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