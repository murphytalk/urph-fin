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
    cash_balance* cash_balances;
};
struct all_brokers{
    broker* brokers;
    int num;
};
all_brokers* get_brokers();
void free_brokers(all_brokers*);

}
#endif // URPH_FIN_CORE_H_
