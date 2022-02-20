#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#endif  // _WIN32

#ifdef _WIN32
#include <windows.h>
#endif  // _WIN32

#include <memory>
#include <stdexcept>

#include "urph-fin-core.h"
#include "firebase/auth.h"
#include "firebase/auth.h"
#include "firebase/auth/credential.h"
#include "firebase/util.h"

using ::firebase::App;
using ::firebase::AppOptions;
using ::firebase::Future;
using ::firebase::FutureBase;
using ::firebase::Variant;
using ::firebase::auth::AdditionalUserInfo;
using ::firebase::auth::Auth;
using ::firebase::auth::AuthError;
using ::firebase::auth::Credential;
using ::firebase::auth::EmailAuthProvider;

static bool quit = false;

bool ProcessEvents(int msec) {
#ifdef _WIN32
  Sleep(msec);
#else
  usleep(msec * 1000);
#endif  // _WIN32
  return quit;
}


class Cloud{
private:
    std::unique_ptr<::firebase::App> _firebaseApp;
    void auth(){

    }
public:
    Cloud(){
        _firebaseApp = std::unique_ptr<::firebase::App>(
#if defined(__ANDROID__)
            App::Create(GetJniEnv(), GetActivity())
#else
            ::firebase::App::Create()
#endif
            );

        ::firebase::ModuleInitializer initializer;
        auto app = _firebaseApp.get();
        initializer.Initialize(app, nullptr, [](::firebase::App* app, void*) {
            ::firebase::InitResult init_result;
            ::Auth::GetAuth(app, &init_result);
            return init_result;
        });

        while (initializer.InitializeLastResult().status() != firebase::kFutureStatusComplete) {
            if (ProcessEvents(100)) throw std::runtime_error("Timeout when wait for firestore app to initialize");
        }

        if (initializer.InitializeLastResult().error() != 0) {
            if (ProcessEvents(200)) throw std::runtime_error("Failed to ini firestore app");
        }
    }
    ~Cloud(){
        
    }
};

static std::unique_ptr<Cloud> cloud;

bool urph_fin_core_init()
{
    if()
    return true;
}

void urph_fin_core_close()
{
}
