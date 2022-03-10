#ifndef URPH_FIN_CORE_H_
#define URPH_FIN_CORE_H_

extern "C"
{

typedef void (*OnInitDone)();

bool urph_fin_core_init(OnInitDone);
void urph_fin_core_close();

}

#endif // URPH_FIN_CORE_H_
