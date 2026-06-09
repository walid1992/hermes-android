#include "hermes_context_jni.h"
#include <jni.h>
#include <android/log.h>
#include <string>

#define TAG "HermesWrapper"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

/**
 * Hermes Android Wrapper - JNI Bridge
 * 
 * This file bridges Java/Kotlin calls to the Hermes JS engine.
 * 
 * Build options:
 * 1. Prebuilt: Link against prebuilt libhermes.so from React Native or Hermes releases
 * 2. Source build: Build Hermes from source via CMake (see README.md)
 * 
 * The current implementation provides stub functions that will be replaced
 * once Hermes headers and libraries are properly linked.
 */

// Forward declaration - will be replaced with actual Hermes types
// when Hermes headers are properly included
struct HermesRuntime {
    void* runtime;
    void* context;
};

static HermesRuntime* createRuntime() {
    LOGI("Creating Hermes runtime (stub)");
    // TODO: Replace with actual Hermes runtime creation:
    // auto runtime = hermes::runtime::make();
    return new HermesRuntime{nullptr, nullptr};
}

static void destroyRuntime(HermesRuntime* rt) {
    if (rt) {
        LOGI("Destroying Hermes runtime");
        delete rt;
    }
}

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_hermes_wrapper_HermesContext_nativeCreate(JNIEnv *env, jobject thiz) {
    HermesRuntime* rt = createRuntime();
    return reinterpret_cast<jlong>(rt);
}

JNIEXPORT void JNICALL
Java_com_hermes_wrapper_HermesContext_nativeDestroy(JNIEnv *env, jobject thiz, jlong handle) {
    HermesRuntime* rt = reinterpret_cast<HermesRuntime*>(handle);
    destroyRuntime(rt);
}

JNIEXPORT jstring JNICALL
Java_com_hermes_wrapper_HermesContext_nativeEval(JNIEnv *env, jobject thiz, jlong handle, jstring code) {
    const char* codeStr = env->GetStringUTFChars(code, nullptr);
    LOGI("Evaluating: %s", codeStr);
    
    // TODO: Replace with actual Hermes eval:
    // auto result = rt->context->evaluateJavaScript(codeStr);
    // return env->NewStringUTF(result.getString().c_str());
    
    std::string result = "[Hermes Stub] Evaluated: " + std::string(codeStr);
    env->ReleaseStringUTFChars(code, codeStr);
    return env->NewStringUTF(result.c_str());
}

JNIEXPORT jboolean JNICALL
Java_com_hermes_wrapper_HermesContext_nativeEvalBoolean(JNIEnv *env, jobject thiz, jlong handle, jstring code) {
    return JNI_TRUE;
}

JNIEXPORT jdouble JNICALL
Java_com_hermes_wrapper_HermesContext_nativeEvalDouble(JNIEnv *env, jobject thiz, jlong handle, jstring code) {
    return 0.0;
}

JNIEXPORT jint JNICALL
Java_com_hermes_wrapper_HermesContext_nativeEvalInt(JNIEnv *env, jobject thiz, jlong handle, jstring code) {
    return 0;
}

JNIEXPORT void JNICALL
Java_com_hermes_wrapper_HermesContext_nativeSetGlobal(JNIEnv *env, jobject thiz, jlong handle, jstring name, jstring value) {
    const char* nameStr = env->GetStringUTFChars(name, nullptr);
    const char* valStr = env->GetStringUTFChars(value, nullptr);
    LOGI("Set global: %s = %s", nameStr, valStr);
    // TODO: Implement with Hermes runtime
    env->ReleaseStringUTFChars(name, nameStr);
    env->ReleaseStringUTFChars(value, valStr);
}

JNIEXPORT jstring JNICALL
Java_com_hermes_wrapper_HermesContext_nativeGetGlobal(JNIEnv *env, jobject thiz, jlong handle, jstring name) {
    return env->NewStringUTF("");
}

JNIEXPORT void JNICALL
Java_com_hermes_wrapper_HermesContext_nativeRunScript(JNIEnv *env, jobject thiz, jlong handle, jstring script) {
    const char* scriptStr = env->GetStringUTFChars(script, nullptr);
    LOGI("Running script: %s", scriptStr);
    // TODO: Implement with Hermes runtime
    env->ReleaseStringUTFChars(script, scriptStr);
}

JNIEXPORT jstring JNICALL
Java_com_hermes_wrapper_HermesContext_nativeCallFunction(JNIEnv *env, jobject thiz, jlong handle, jstring funcName, jstring argsJson) {
    return env->NewStringUTF("{}");
}

} // extern "C"
