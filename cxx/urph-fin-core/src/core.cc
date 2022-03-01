#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#endif  // _WIN32

#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif  // _WIN32

#include <memory>
#include <stdexcept>
#include <iostream>

#include "urph-fin-core.h"
#include "aixlog.hpp"
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
    App*  _firebaseApp;
    Auth* _firebaseAuth;
    void auth(){

    }
public:
    Cloud(){
        _firebaseApp = 
#if defined(__ANDROID__)
            App::Create(GetJniEnv(), GetActivity())
#else
            ::firebase::App::Create()
#endif
        ;

        ::firebase::ModuleInitializer initializer;
        initializer.Initialize(_firebaseApp, nullptr, [](::firebase::App* app, void*) {
            ::firebase::InitResult init_result;
            Auth::GetAuth(app, &init_result);
            return init_result;
        });

        while (initializer.InitializeLastResult().status() != firebase::kFutureStatusComplete) {
            if (ProcessEvents(100)) throw std::runtime_error("Timeout when wait for firestore app to initialize");
        }

        if (initializer.InitializeLastResult().error() != 0) {
            if (ProcessEvents(200)) throw std::runtime_error("Failed to ini firestore app");
        }

        _firebaseAuth = Auth::GetAuth(_firebaseApp);

        LOG(DEBUG) << "Cloud instance created\n";
    }
    ~Cloud(){
        delete _firebaseAuth;
        delete _firebaseApp;
        LOG(DEBUG) << "Cloud instance destroied\n";
    }
};

//static std::unique_ptr<Cloud> cloud;
Cloud *cloud;

bool urph_fin_core_init()
{
    std::vector<AixLog::log_sink_ptr> sinks;
    
    auto log_file = getenv("LOGFILE");
    if(log_file == nullptr)
        sinks.push_back(std::make_shared<AixLog::SinkCout>(AixLog::Severity::debug));
    else  
        sinks.push_back(std::make_shared<AixLog::SinkFile>(AixLog::Severity::debug, log_file)); 
    AixLog::Log::init(sinks);
    
    LOG(DEBUG) << "urph-fin-core initializing\n";
    try{
        cloud = new Cloud();
    }
    catch(const std::runtime_error& e){
        LOG(ERROR) << "Failed to init cloud service: " << e.what() << "\n";
    }
    return true;
}

void urph_fin_core_close()
{
    delete cloud;
}
