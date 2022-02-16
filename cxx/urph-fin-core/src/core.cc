#include "urph-fin-core.h"
#include <memory>
#include "firebase/auth.h"

bool urph_fin_core_init()
{
    auto _pFirebaseApp = std::unique_ptr<::firebase::App>(::firebase::App::Create());
    return true;
}

void urph_fin_core_close()
{
}
