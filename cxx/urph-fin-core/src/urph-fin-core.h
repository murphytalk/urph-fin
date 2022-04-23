#ifndef URPH_FIN_CORE_H_
#define URPH_FIN_CORE_H_

extern "C"
{

typedef void (*OnInitDone)();

bool urph_fin_core_init(OnInitDone);
void urph_fin_core_close();

struct cash_balance{
    char* ccy;
    double balance;
};
struct broker{
    char* name;
    int num;
    cash_balance* cash_balance_ptrs;
};
struct brokers{
    broker* broker_ptrs;
    int num;
};
brokers* get_brokers();
void free_brokers(brokers*);

}
#endif // URPH_FIN_CORE_H_
