#include <jni.h>
//#include <native_app_glue/android_native_app_glue.h>

// Get the JNI environment.
JNIEnv* GetJniEnv() {
  JavaVM* vm = g_app_state->activity->vm;
  JNIEnv* env;
  jint result = vm->AttachCurrentThread(&env, nullptr);
  return result == JNI_OK ? env : nullptr;
}

// Get the activity.
jobject GetActivity() { return g_app_state->activity->clazz; }
