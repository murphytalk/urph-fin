#include <sys/types.h>
#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#include <pwd.h>
#endif  // _WIN32


#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#endif  // _WIN32

#include <memory>
#include <stdexcept>
#include <iostream>
#include <string>

// 3rd party
#include "aixlog.hpp"
#include "yaml-cpp/yaml.h"

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
using ::firebase::auth::User;
using ::firebase::auth::AuthError;
using ::firebase::auth::Credential;
using ::firebase::auth::EmailAuthProvider;


static bool quit = false;

static bool ProcessEvents(int msec) {
#ifdef _WIN32
  Sleep(msec);
#else
  usleep(msec * 1000);
#endif  // _WIN32
  return quit;
}

static std::string get_home_dir()
{
    const char *homedir;
#ifdef _WIN32
    //HOMEPATH or userprofile
#else
    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }
#endif
    // compiler is smart enough to either do return-value-optimization or use move
    //https://stackoverflow.com/questions/4986673/c11-rvalues-and-move-semantics-confusion-return-statement
    return std::string(homedir);
}

static YAML::Node load_cfg(){
    return YAML::LoadFile(get_home_dir() + "/.finance-credentials/urph-fin.yaml");
}


class Cloud{
private:
    App*  _firebaseApp;
    Auth* _firebaseAuth;
    void auth(){

    }
public:
    Cloud(OnInitDone onInitDone){
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

        auto c = [](const Future<User*>& u, void* onInitDone){
            auto err_code = u.error();
            if(err_code == 0){
                LOG(INFO) << "Auth completed\n";
                (*(OnInitDone)onInitDone)();
            }
            else{
                std::ostringstream ss;
                ss << "Failed to auth : err code - " << err_code << ", err msg - " <<  u.error_message();
                throw std::runtime_error(ss.str());
            }

        };
#if defined(__ANDROID__)
#else
        auto cfg = load_cfg();
        auto userCfg = cfg["user"];
        auto email = userCfg["email"].as<std::string>();
        auto passwd = userCfg["password"].as<std::string>();
        LOG(INFO) << "Log in as " << email << "\n"; 
        _firebaseAuth->SignInWithEmailAndPassword(email.c_str(), passwd.c_str())
#endif
        .OnCompletion(c, (void*)onInitDone);
        
    }
    ~Cloud(){
        _firebaseAuth->SignOut();
        delete _firebaseAuth;
        delete _firebaseApp;
        LOG(DEBUG) << "Cloud instance destroied\n";
    }
};

Cloud *cloud;

bool urph_fin_core_init(OnInitDone onInitDone)
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
        cloud = new Cloud(onInitDone);
        return true;
    }
    catch(const std::exception& e){
        LOG(ERROR) << "Failed to init cloud service: " << e.what() << "\n";
        return false;
    }
}

void urph_fin_core_close()
{
    delete cloud;
}
