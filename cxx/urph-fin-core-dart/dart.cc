#include <functional>
#include <iostream>

#include <dart_api_dl.h>
#include <core/urph-fin-core.h>

// https://github.com/dart-lang/sdk/blob/main/runtime/bin/ffi_test/ffi_test_functions_vmspecific.cc

DART_EXPORT intptr_t InitializeDartApiDL(void *data)
{
    return Dart_InitializeApiDL(data);
}

static Dart_Port send_port_;

static OnDone on_init_done_callback;
DART_EXPORT void register_init_callback(Dart_Port send_port, OnDone cb)
{
    send_port_ = send_port;
    on_init_done_callback = cb;
}

static OnAssetLoaded on_asset_loaded_callback;
DART_EXPORT void register_asset_loaded_callback(OnAssetLoaded cb)
{
    on_asset_loaded_callback = cb;
}


typedef std::function<void()> Work;
// Notify Dart through a port that the C lib has pending async callbacks.
//
// Expects heap allocated `work` so delete can be called on it.
//
// The `send_port` should be from the dart isolate which registered the callback.
static void notify_dart(Dart_Port send_port, const Work *work)
{
    const intptr_t work_addr = reinterpret_cast<intptr_t>(work);
    std::cout << "C   :  Posting message port=" << send_port << ",work=" << work << std::endl;

    Dart_CObject dart_object;
    dart_object.type = Dart_CObject_kInt64;
    dart_object.value.as_int64 = work_addr;

    const bool result = Dart_PostCObject_DL(send_port, &dart_object);
    if (!result)
    {
        std::cerr << "C   :  Posting message to port failed." << std::endl;
    }
}

DART_EXPORT bool dart_urph_fin_core_init()
{
    return urph_fin_core_init([](void *p){
        const Work work = [p](){on_init_done_callback(p);};
        // Copy to heap to make it outlive the function scope.
        const Work* work_ptr = new Work(work);
        notify_dart(send_port_, work_ptr); 
    }, nullptr);
}

DART_EXPORT void dart_urph_fin_load_assets()
{
    load_assets([](void *p, AssetHandle h){
        const Work work = [p,h](){on_asset_loaded_callback(p,h);};
        // Copy to heap to make it outlive the function scope.
        const Work* work_ptr = new Work(work);
        notify_dart(send_port_, work_ptr); 
    },nullptr);
}


// execute the dart side registered callback in the dart isolate which initiates the native call
//      Dart isolate                          Native thread
// 
//      native_call(callback)
//      [native code] req to process data     
//                                            processing finished, got data
//                                            wraps callback in work and capture the parameters (result)
//                                            notify dart with the work pointer
//      [native code] execute_callback
//
DART_EXPORT void execute_callback(Work *work_ptr)
{
    std::cout << "C Da:    ExecuteCallback(" << reinterpret_cast<intptr_t>(work_ptr) << std::endl;
    const Work work = *work_ptr;
    work();
    delete work_ptr;
    std::cout << "C Da:    ExecuteCallback done" << std::endl;
}