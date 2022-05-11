 #include "stock.hxx"
 #include <exception>
 #include <deque>
 #include <numeric>
 #include <execution>
 #include <cstring>

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

StockTx::StockTx(const std::string& b, double s, double p, double f, const std::string sd)
{
    broker = copy_str(b);
    shares = s;
    price = p;
    fee = f;
    side = sd == "BUY" ? BUY : ( sd == "SELL" ? SELL : SPLIT);
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
}

StockWithTx::StockWithTx(stock* i, stock_tx_list* t)
{
    instrument = i;
    tx_list = t;
}

StockWithTx::~StockWithTx()
{
    delete instrument;
    delete tx_list;
}

StockPortfolio::StockPortfolio(int n, stock_with_tx* first)
{
    num = n;
    first_stock_with_tx = first;
}

StockPortfolio::~StockPortfolio()
{
    free_placement_allocated_structs<StockPortfolio, StockWithTx>(this);
}

class UnclosedPosition{
public:
    double price;
    double shares;
    double fee;

    void split(double factor){
        price = price / factor;
        shares = shares * factor;
    }
};

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
                auto& first_unclosed_pos = unclosed_positions.front();
                for(auto shares = tx.shares; shares > 0;){
                    if(unclosed_positions.empty()){
                        throw std::runtime_error("Not enough unclosed position");
                    }
                    else if (first_unclosed_pos.shares > shares){
                        first_unclosed_pos.shares -= shares;
                        shares = 0;
                    }
                    else if (first_unclosed_pos.shares == shares){
                        unclosed_positions.pop_front();
                        shares = 0;
                    }
                    else{
                        unclosed_positions.pop_front();
                        shares -= first_unclosed_pos.shares;
                    }
                }
            }
        }
        auto r = std::reduce(std::execution::par, unclosed_positions.begin(), unclosed_positions.end(), UnclosedPosition{0.0, 0.0, 0.0},
            [](UnclosedPosition& a, UnclosedPosition& x){
                a.fee += x.fee;
                a.price += x.price * x.shares;
                a.shares += x.shares;
                return a;
            }
        );
        balance.vwap = r.shares == 0 ? 0 : (r.price + r.fee) / r.shares;
    }
    return balance;
}