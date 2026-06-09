#include <jni.h>
#include <android/log.h>
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>

#include <hermes/hermes.h>
#include <jsi/jsi.h>

#define TAG "HermesWrapper"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)

using namespace facebook::jsi;
using namespace facebook::hermes;

// ============================================================================
// Cached JNI References — resolved once at JNI_OnLoad, avoid repeated lookups
// ============================================================================

struct JniCache {
    JavaVM* jvm = nullptr;

    // Java types
    jclass stringCls = nullptr;
    jclass integerCls = nullptr;
    jclass longCls = nullptr;
    jclass doubleCls = nullptr;
    jclass floatCls = nullptr;
    jclass booleanCls = nullptr;
    jclass objectCls = nullptr;

    // Wrapper types
    jclass jsObjectCls = nullptr;
    jclass jsArrayCls = nullptr;
    jclass jsFunctionCls = nullptr;
    jclass jsCallFunctionCls = nullptr;

    // Factory methods
    jmethodID booleanValueOf = nullptr;
    jmethodID doubleValueOf = nullptr;

    // Unbox methods
    jmethodID intValue = nullptr;
    jmethodID longValue = nullptr;
    jmethodID doubleValue = nullptr;
    jmethodID floatValue = nullptr;
    jmethodID booleanValue = nullptr;

    // Wrapper constructors
    jmethodID jsObjectInit = nullptr;
    jmethodID jsArrayInit = nullptr;
    jmethodID jsFunctionInit = nullptr;

    // Wrapper fields
    jfieldID jsObjectHandle = nullptr;

    // Callback method
    jmethodID jsCallFunctionCall = nullptr;

    bool init(JNIEnv* env) {
        // Java boxed types (make global refs so they survive across frames)
        stringCls = (jclass)env->NewGlobalRef(env->FindClass("java/lang/String"));
        integerCls = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Integer"));
        longCls = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Long"));
        doubleCls = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Double"));
        floatCls = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Float"));
        booleanCls = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Boolean"));
        objectCls = (jclass)env->NewGlobalRef(env->FindClass("java/lang/Object"));

        // Wrapper types
        jsObjectCls = (jclass)env->NewGlobalRef(env->FindClass("com/hermes/wrapper/JSObject"));
        jsArrayCls = (jclass)env->NewGlobalRef(env->FindClass("com/hermes/wrapper/JSArray"));
        jsFunctionCls = (jclass)env->NewGlobalRef(env->FindClass("com/hermes/wrapper/JSFunction"));
        jsCallFunctionCls = (jclass)env->NewGlobalRef(env->FindClass("com/hermes/wrapper/JSCallFunction"));

        // Method IDs (these are stable, don't need global ref)
        booleanValueOf = env->GetStaticMethodID(booleanCls, "valueOf", "(Z)Ljava/lang/Boolean;");
        doubleValueOf = env->GetStaticMethodID(doubleCls, "valueOf", "(D)Ljava/lang/Double;");

        intValue = env->GetMethodID(integerCls, "intValue", "()I");
        longValue = env->GetMethodID(longCls, "longValue", "()J");
        doubleValue = env->GetMethodID(doubleCls, "doubleValue", "()D");
        floatValue = env->GetMethodID(floatCls, "floatValue", "()F");
        booleanValue = env->GetMethodID(booleanCls, "booleanValue", "()Z");

        jsObjectInit = env->GetMethodID(jsObjectCls, "<init>", "(JJ)V");
        jsArrayInit = env->GetMethodID(jsArrayCls, "<init>", "(JJ)V");
        jsFunctionInit = env->GetMethodID(jsFunctionCls, "<init>", "(JJ)V");

        jsObjectHandle = env->GetFieldID(jsObjectCls, "nativeHandle", "J");

        jsCallFunctionCall = env->GetMethodID(jsCallFunctionCls, "call",
            "([Ljava/lang/Object;)Ljava/lang/Object;");

        return true;
    }
};

static JniCache g_cache;

// ============================================================================
// Object Store: maps integer handles to JSI Object pointers
// ============================================================================

struct HermesContextData {
    std::unique_ptr<HermesRuntime> runtime;
    std::unordered_map<long, std::shared_ptr<Object>> objectStore;
    std::unordered_set<jobject> globalRefs;  // Track callback refs for cleanup
    long nextHandle = 1;
    std::mutex mtx;

    long storeObject(Object&& obj) {
        std::lock_guard<std::mutex> lock(mtx);
        long handle = nextHandle++;
        objectStore[handle] = std::make_shared<Object>(std::move(obj));
        return handle;
    }

    Object* getObject(long handle) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = objectStore.find(handle);
        return (it != objectStore.end()) ? it->second.get() : nullptr;
    }

    void releaseObject(long handle) {
        std::lock_guard<std::mutex> lock(mtx);
        objectStore.erase(handle);
    }

    void trackGlobalRef(jobject ref) {
        std::lock_guard<std::mutex> lock(mtx);
        globalRefs.insert(ref);
    }

    void destroy(JNIEnv* env) {
        std::lock_guard<std::mutex> lock(mtx);
        // Release all stored JS objects before runtime destruction
        objectStore.clear();
        // Delete all Java global refs to prevent leaks
        for (jobject ref : globalRefs) {
            env->DeleteGlobalRef(ref);
        }
        globalRefs.clear();
        runtime.reset();
    }
};

// ============================================================================
// Helpers
// ============================================================================

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_cache.jvm = vm;
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) {
        g_cache.init(env);
    }
    return JNI_VERSION_1_6;
}

static JNIEnv* getEnv() {
    JNIEnv* env = nullptr;
    if (!g_cache.jvm) return nullptr;
    jint status = g_cache.jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (status == JNI_EDETACHED) {
        // Attach native thread if needed (e.g., timer callbacks)
        if (g_cache.jvm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
            return nullptr;
        }
    }
    return env;
}

static inline std::string jstringToString(JNIEnv* env, jstring str) {
    if (!str) return "";
    const char* chars = env->GetStringUTFChars(str, nullptr);
    std::string result(chars);
    env->ReleaseStringUTFChars(str, chars);
    return result;
}

// Macro to reduce boilerplate: get context data and runtime, return retval on failure
#define GET_CTX_RT(handle, retval) \
    auto* data = reinterpret_cast<HermesContextData*>(handle); \
    if (!data || !data->runtime) return retval; \
    auto& rt = *data->runtime;

#define GET_CTX_RT_OBJ(ctxHandle, objHandle, retval) \
    auto* data = reinterpret_cast<HermesContextData*>(ctxHandle); \
    if (!data || !data->runtime) return retval; \
    auto& rt = *data->runtime; \
    Object* obj = data->getObject(objHandle); \
    if (!obj) return retval;

// ============================================================================
// Type Conversion: JSI <-> Java
// ============================================================================

static jobject valueToJava(JNIEnv* env, Runtime& rt, const Value& val, HermesContextData* ctx) {
    if (val.isNull() || val.isUndefined()) {
        return nullptr;
    } else if (val.isBool()) {
        return env->CallStaticObjectMethod(g_cache.booleanCls, g_cache.booleanValueOf,
            val.getBool() ? JNI_TRUE : JNI_FALSE);
    } else if (val.isNumber()) {
        return env->CallStaticObjectMethod(g_cache.doubleCls, g_cache.doubleValueOf,
            val.getNumber());
    } else if (val.isString()) {
        std::string s = val.getString(rt).utf8(rt);
        return env->NewStringUTF(s.c_str());
    } else if (val.isObject()) {
        Object obj = val.getObject(rt);
        // Check type before storing (avoids extra getObject lookup)
        bool isArr = obj.isArray(rt);
        bool isFunc = !isArr && obj.isFunction(rt);
        long handle = ctx->storeObject(std::move(obj));
        jlong ctxPtr = reinterpret_cast<jlong>(ctx);

        if (isArr) {
            return env->NewObject(g_cache.jsArrayCls, g_cache.jsArrayInit, ctxPtr, (jlong)handle);
        } else if (isFunc) {
            return env->NewObject(g_cache.jsFunctionCls, g_cache.jsFunctionInit, ctxPtr, (jlong)handle);
        } else {
            return env->NewObject(g_cache.jsObjectCls, g_cache.jsObjectInit, ctxPtr, (jlong)handle);
        }
    }
    return nullptr;
}

static Value javaToValue(JNIEnv* env, Runtime& rt, jobject obj, HermesContextData* ctx) {
    if (!obj) return Value::null();

    if (env->IsInstanceOf(obj, g_cache.stringCls)) {
        std::string s = jstringToString(env, (jstring)obj);
        return String::createFromUtf8(rt, s);
    } else if (env->IsInstanceOf(obj, g_cache.integerCls)) {
        return Value((double)env->CallIntMethod(obj, g_cache.intValue));
    } else if (env->IsInstanceOf(obj, g_cache.longCls)) {
        return Value((double)env->CallLongMethod(obj, g_cache.longValue));
    } else if (env->IsInstanceOf(obj, g_cache.doubleCls)) {
        return Value(env->CallDoubleMethod(obj, g_cache.doubleValue));
    } else if (env->IsInstanceOf(obj, g_cache.floatCls)) {
        return Value((double)env->CallFloatMethod(obj, g_cache.floatValue));
    } else if (env->IsInstanceOf(obj, g_cache.booleanCls)) {
        return Value(env->CallBooleanMethod(obj, g_cache.booleanValue) == JNI_TRUE);
    } else if (env->IsInstanceOf(obj, g_cache.jsObjectCls)) {
        long handle = (long)env->GetLongField(obj, g_cache.jsObjectHandle);
        Object* stored = ctx->getObject(handle);
        if (stored) return Value(rt, *stored);
    } else if (env->IsInstanceOf(obj, g_cache.jsCallFunctionCls)) {
        // Wrap Java callback as a JS HostFunction
        jobject callbackRef = env->NewGlobalRef(obj);
        ctx->trackGlobalRef(callbackRef);

        auto hostFn = Function::createFromHostFunction(rt,
            PropNameID::forUtf8(rt, "javaCallback"), 10,
            [ctx, callbackRef](Runtime& rt, const Value& thisVal,
                               const Value* args, size_t count) -> Value {
                JNIEnv* env = getEnv();
                if (!env) return Value::undefined();

                jobjectArray jArgs = env->NewObjectArray(count, g_cache.objectCls, nullptr);
                for (size_t i = 0; i < count; i++) {
                    jobject jArg = valueToJava(env, rt, args[i], ctx);
                    env->SetObjectArrayElement(jArgs, i, jArg);
                    if (jArg) env->DeleteLocalRef(jArg);
                }

                jobject jResult = env->CallObjectMethod(callbackRef,
                    g_cache.jsCallFunctionCall, jArgs);

                // Check for Java exception
                if (env->ExceptionCheck()) {
                    env->ExceptionDescribe();
                    env->ExceptionClear();
                    env->DeleteLocalRef(jArgs);
                    return Value::undefined();
                }

                Value result = javaToValue(env, rt, jResult, ctx);
                env->DeleteLocalRef(jArgs);
                if (jResult) env->DeleteLocalRef(jResult);
                return result;
            });
        return Value(rt, std::move(hostFn));
    }

    return Value::null();
}

// Helper: invoke a Java callback from a host function (shared between setPropertyFunction and javaToValue)
static Function createHostFunction(JNIEnv* env, Runtime& rt, HermesContextData* data,
                                    const std::string& name, jobject javaCallback) {
    jobject callbackRef = env->NewGlobalRef(javaCallback);
    data->trackGlobalRef(callbackRef);

    return Function::createFromHostFunction(rt,
        PropNameID::forUtf8(rt, name), 10,
        [data, callbackRef](Runtime& rt, const Value& thisVal,
                           const Value* args, size_t count) -> Value {
            JNIEnv* env = getEnv();
            if (!env) return Value::undefined();

            jobjectArray jArgs = env->NewObjectArray(count, g_cache.objectCls, nullptr);
            for (size_t i = 0; i < count; i++) {
                jobject jArg = valueToJava(env, rt, args[i], data);
                env->SetObjectArrayElement(jArgs, i, jArg);
                if (jArg) env->DeleteLocalRef(jArg);
            }

            jobject jResult = env->CallObjectMethod(callbackRef,
                g_cache.jsCallFunctionCall, jArgs);

            if (env->ExceptionCheck()) {
                env->ExceptionDescribe();
                env->ExceptionClear();
                env->DeleteLocalRef(jArgs);
                return Value::undefined();
            }

            Value result = javaToValue(env, rt, jResult, data);
            env->DeleteLocalRef(jArgs);
            if (jResult) env->DeleteLocalRef(jResult);
            return result;
        });
}

// ============================================================================
// HermesContext JNI
// ============================================================================

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_hermes_wrapper_HermesContext_nativeCreate(JNIEnv *env, jobject thiz) {
    try {
        auto data = new HermesContextData();
        data->runtime = makeHermesRuntime();
        LOGI("Hermes runtime created successfully");
        return reinterpret_cast<jlong>(data);
    } catch (const std::exception &e) {
        LOGE("Failed to create Hermes runtime: %s", e.what());
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"),
                      (std::string("Failed to create Hermes runtime: ") + e.what()).c_str());
        return 0;
    }
}

JNIEXPORT void JNICALL
Java_com_hermes_wrapper_HermesContext_nativeDestroy(JNIEnv *env, jobject thiz, jlong handle) {
    auto data = reinterpret_cast<HermesContextData *>(handle);
    if (data) {
        LOGI("Destroying Hermes runtime (releasing %zu objects, %zu refs)",
             data->objectStore.size(), data->globalRefs.size());
        data->destroy(env);
        delete data;
    }
}

JNIEXPORT jlong JNICALL
Java_com_hermes_wrapper_HermesContext_nativeGetGlobalObject(JNIEnv *env, jobject thiz, jlong handle) {
    GET_CTX_RT(handle, 0);
    Object global = rt.global();
    return data->storeObject(std::move(global));
}

JNIEXPORT jlong JNICALL
Java_com_hermes_wrapper_HermesContext_nativeCreateObject(JNIEnv *env, jobject thiz, jlong handle) {
    GET_CTX_RT(handle, 0);
    Object obj(rt);
    return data->storeObject(std::move(obj));
}

JNIEXPORT jlong JNICALL
Java_com_hermes_wrapper_HermesContext_nativeCreateArray(JNIEnv *env, jobject thiz, jlong handle) {
    GET_CTX_RT(handle, 0);
    Array arr = Array(rt, 0);
    return data->storeObject(std::move(arr));
}

JNIEXPORT jstring JNICALL
Java_com_hermes_wrapper_HermesContext_nativeEval(JNIEnv *env, jobject thiz, jlong handle, jstring code) {
    GET_CTX_RT(handle, nullptr);
    std::string codeString = jstringToString(env, code);

    try {
        Value result = rt.evaluateJavaScript(
            std::make_shared<StringBuffer>(codeString), "<eval>");

        std::string resultStr;
        if (result.isUndefined()) {
            resultStr = "undefined";
        } else if (result.isNull()) {
            resultStr = "null";
        } else if (result.isBool()) {
            resultStr = result.getBool() ? "true" : "false";
        } else if (result.isNumber()) {
            double num = result.getNumber();
            if (num == (int64_t)num && num >= -1e15 && num <= 1e15) {
                resultStr = std::to_string((int64_t)num);
            } else {
                resultStr = std::to_string(num);
            }
        } else if (result.isString()) {
            resultStr = result.getString(rt).utf8(rt);
        } else if (result.isObject()) {
            auto JSON = rt.global().getPropertyAsObject(rt, "JSON");
            auto stringify = JSON.getPropertyAsFunction(rt, "stringify");
            Value jsonResult = stringify.call(rt, result);
            resultStr = jsonResult.isString() ? jsonResult.getString(rt).utf8(rt) : "[object]";
        } else {
            resultStr = "[unknown]";
        }
        return env->NewStringUTF(resultStr.c_str());
    } catch (const JSError &e) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"),
                      (std::string("JSError: ") + e.getMessage()).c_str());
        return nullptr;
    } catch (const std::exception &e) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
        return nullptr;
    }
}

JNIEXPORT jboolean JNICALL
Java_com_hermes_wrapper_HermesContext_nativeEvalBoolean(JNIEnv *env, jobject thiz, jlong handle, jstring code) {
    GET_CTX_RT(handle, JNI_FALSE);
    std::string codeString = jstringToString(env, code);
    try {
        Value result = rt.evaluateJavaScript(std::make_shared<StringBuffer>(codeString), "<eval>");
        if (result.isBool()) return result.getBool() ? JNI_TRUE : JNI_FALSE;
        if (result.isNumber()) return result.getNumber() != 0 ? JNI_TRUE : JNI_FALSE;
        if (result.isNull() || result.isUndefined()) return JNI_FALSE;
        return JNI_TRUE;
    } catch (...) {
        return JNI_FALSE;
    }
}

JNIEXPORT jdouble JNICALL
Java_com_hermes_wrapper_HermesContext_nativeEvalDouble(JNIEnv *env, jobject thiz, jlong handle, jstring code) {
    GET_CTX_RT(handle, 0.0);
    std::string codeString = jstringToString(env, code);
    try {
        Value result = rt.evaluateJavaScript(std::make_shared<StringBuffer>(codeString), "<eval>");
        return result.isNumber() ? result.getNumber() : 0.0;
    } catch (...) {
        return 0.0;
    }
}

JNIEXPORT jint JNICALL
Java_com_hermes_wrapper_HermesContext_nativeEvalInt(JNIEnv *env, jobject thiz, jlong handle, jstring code) {
    GET_CTX_RT(handle, 0);
    std::string codeString = jstringToString(env, code);
    try {
        Value result = rt.evaluateJavaScript(std::make_shared<StringBuffer>(codeString), "<eval>");
        return result.isNumber() ? (jint)result.getNumber() : 0;
    } catch (...) {
        return 0;
    }
}

JNIEXPORT void JNICALL
Java_com_hermes_wrapper_HermesContext_nativeRunScript(JNIEnv *env, jobject thiz, jlong handle, jstring script) {
    GET_CTX_RT(handle, );
    std::string scriptString = jstringToString(env, script);
    try {
        rt.evaluateJavaScript(std::make_shared<StringBuffer>(scriptString), "<script>");
    } catch (const JSError &e) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"),
                      (std::string("JSError: ") + e.getMessage()).c_str());
    } catch (const std::exception &e) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    }
}

// ============================================================================
// JSObject JNI — Set Property
// ============================================================================

JNIEXPORT void JNICALL
Java_com_hermes_wrapper_JSObject_nativeSetPropertyString(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong objHandle, jstring name, jstring value) {
    GET_CTX_RT_OBJ(ctxHandle, objHandle, );
    std::string nameStr = jstringToString(env, name);
    try {
        if (value) {
            std::string valStr = jstringToString(env, value);
            obj->setProperty(rt, PropNameID::forUtf8(rt, nameStr),
                           String::createFromUtf8(rt, valStr));
        } else {
            obj->setProperty(rt, PropNameID::forUtf8(rt, nameStr), Value::null());
        }
    } catch (const std::exception &e) {
        LOGE("setPropertyString error: %s", e.what());
    }
}

JNIEXPORT void JNICALL
Java_com_hermes_wrapper_JSObject_nativeSetPropertyInt(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong objHandle, jstring name, jint value) {
    GET_CTX_RT_OBJ(ctxHandle, objHandle, );
    std::string nameStr = jstringToString(env, name);
    try {
        obj->setProperty(rt, PropNameID::forUtf8(rt, nameStr), Value((double)value));
    } catch (const std::exception &e) {
        LOGE("setPropertyInt error: %s", e.what());
    }
}

JNIEXPORT void JNICALL
Java_com_hermes_wrapper_JSObject_nativeSetPropertyDouble(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong objHandle, jstring name, jdouble value) {
    GET_CTX_RT_OBJ(ctxHandle, objHandle, );
    std::string nameStr = jstringToString(env, name);
    try {
        obj->setProperty(rt, PropNameID::forUtf8(rt, nameStr), Value(value));
    } catch (const std::exception &e) {
        LOGE("setPropertyDouble error: %s", e.what());
    }
}

JNIEXPORT void JNICALL
Java_com_hermes_wrapper_JSObject_nativeSetPropertyBoolean(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong objHandle, jstring name, jboolean value) {
    GET_CTX_RT_OBJ(ctxHandle, objHandle, );
    std::string nameStr = jstringToString(env, name);
    try {
        obj->setProperty(rt, PropNameID::forUtf8(rt, nameStr), Value(value == JNI_TRUE));
    } catch (const std::exception &e) {
        LOGE("setPropertyBoolean error: %s", e.what());
    }
}

JNIEXPORT void JNICALL
Java_com_hermes_wrapper_JSObject_nativeSetPropertyObject(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong objHandle, jstring name, jlong valueHandle) {
    GET_CTX_RT_OBJ(ctxHandle, objHandle, );
    Object* valObj = data->getObject(valueHandle);
    if (!valObj) return;

    std::string nameStr = jstringToString(env, name);
    try {
        obj->setProperty(rt, PropNameID::forUtf8(rt, nameStr), Value(rt, *valObj));
    } catch (const std::exception &e) {
        LOGE("setPropertyObject error: %s", e.what());
    }
}

JNIEXPORT void JNICALL
Java_com_hermes_wrapper_JSObject_nativeSetPropertyFunction(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong objHandle, jstring name, jobject javaCallback) {
    GET_CTX_RT_OBJ(ctxHandle, objHandle, );
    std::string nameStr = jstringToString(env, name);

    try {
        Function hostFn = createHostFunction(env, rt, data, nameStr, javaCallback);
        obj->setProperty(rt, PropNameID::forUtf8(rt, nameStr), std::move(hostFn));
    } catch (const std::exception &e) {
        LOGE("setPropertyFunction error: %s", e.what());
    }
}

// ============================================================================
// JSObject JNI — Get Property
// ============================================================================

JNIEXPORT jstring JNICALL
Java_com_hermes_wrapper_JSObject_nativeGetPropertyString(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong objHandle, jstring name) {
    GET_CTX_RT_OBJ(ctxHandle, objHandle, nullptr);
    std::string nameStr = jstringToString(env, name);
    try {
        Value val = obj->getProperty(rt, PropNameID::forUtf8(rt, nameStr));
        if (val.isString()) {
            return env->NewStringUTF(val.getString(rt).utf8(rt).c_str());
        } else if (val.isNumber()) {
            double num = val.getNumber();
            std::string s = (num == (int64_t)num) ? std::to_string((int64_t)num) : std::to_string(num);
            return env->NewStringUTF(s.c_str());
        } else if (val.isBool()) {
            return env->NewStringUTF(val.getBool() ? "true" : "false");
        }
        return nullptr;
    } catch (...) {
        return nullptr;
    }
}

JNIEXPORT jint JNICALL
Java_com_hermes_wrapper_JSObject_nativeGetPropertyInt(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong objHandle, jstring name) {
    GET_CTX_RT_OBJ(ctxHandle, objHandle, 0);
    std::string nameStr = jstringToString(env, name);
    try {
        Value val = obj->getProperty(rt, PropNameID::forUtf8(rt, nameStr));
        return val.isNumber() ? (jint)val.getNumber() : 0;
    } catch (...) {
        return 0;
    }
}

JNIEXPORT jdouble JNICALL
Java_com_hermes_wrapper_JSObject_nativeGetPropertyDouble(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong objHandle, jstring name) {
    GET_CTX_RT_OBJ(ctxHandle, objHandle, 0.0);
    std::string nameStr = jstringToString(env, name);
    try {
        Value val = obj->getProperty(rt, PropNameID::forUtf8(rt, nameStr));
        return val.isNumber() ? val.getNumber() : 0.0;
    } catch (...) {
        return 0.0;
    }
}

JNIEXPORT jboolean JNICALL
Java_com_hermes_wrapper_JSObject_nativeGetPropertyBoolean(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong objHandle, jstring name) {
    GET_CTX_RT_OBJ(ctxHandle, objHandle, JNI_FALSE);
    std::string nameStr = jstringToString(env, name);
    try {
        Value val = obj->getProperty(rt, PropNameID::forUtf8(rt, nameStr));
        return val.isBool() ? (val.getBool() ? JNI_TRUE : JNI_FALSE) : JNI_FALSE;
    } catch (...) {
        return JNI_FALSE;
    }
}

JNIEXPORT jlong JNICALL
Java_com_hermes_wrapper_JSObject_nativeGetPropertyObject(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong objHandle, jstring name) {
    GET_CTX_RT_OBJ(ctxHandle, objHandle, 0);
    std::string nameStr = jstringToString(env, name);
    try {
        Value val = obj->getProperty(rt, PropNameID::forUtf8(rt, nameStr));
        if (val.isObject()) {
            Object child = val.getObject(rt);
            return data->storeObject(std::move(child));
        }
        return 0;
    } catch (...) {
        return 0;
    }
}

JNIEXPORT jboolean JNICALL
Java_com_hermes_wrapper_JSObject_nativeHasProperty(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong objHandle, jstring name) {
    GET_CTX_RT_OBJ(ctxHandle, objHandle, JNI_FALSE);
    std::string nameStr = jstringToString(env, name);
    try {
        return obj->hasProperty(rt, PropNameID::forUtf8(rt, nameStr)) ? JNI_TRUE : JNI_FALSE;
    } catch (...) {
        return JNI_FALSE;
    }
}

JNIEXPORT void JNICALL
Java_com_hermes_wrapper_JSObject_nativeRelease(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong objHandle) {
    auto data = reinterpret_cast<HermesContextData *>(ctxHandle);
    if (data) data->releaseObject(objHandle);
}

// ============================================================================
// JSArray JNI
// ============================================================================

JNIEXPORT jint JNICALL
Java_com_hermes_wrapper_JSArray_nativeArrayLength(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong objHandle) {
    GET_CTX_RT_OBJ(ctxHandle, objHandle, 0);
    try {
        if (!obj->isArray(rt)) return 0;
        Array arr = obj->getArray(rt);
        return (jint)arr.size(rt);
    } catch (...) {
        return 0;
    }
}

JNIEXPORT jobject JNICALL
Java_com_hermes_wrapper_JSArray_nativeArrayGet(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong objHandle, jint index) {
    GET_CTX_RT_OBJ(ctxHandle, objHandle, nullptr);
    try {
        if (!obj->isArray(rt)) return nullptr;
        Array arr = obj->getArray(rt);
        Value val = arr.getValueAtIndex(rt, index);
        return valueToJava(env, rt, val, data);
    } catch (...) {
        return nullptr;
    }
}

JNIEXPORT void JNICALL
Java_com_hermes_wrapper_JSArray_nativeArraySet(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong objHandle, jint index, jobject value) {
    GET_CTX_RT_OBJ(ctxHandle, objHandle, );
    try {
        if (!obj->isArray(rt)) return;
        Array arr = obj->getArray(rt);
        Value val = javaToValue(env, rt, value, data);
        arr.setValueAtIndex(rt, index, std::move(val));
    } catch (const std::exception &e) {
        LOGE("arraySet error: %s", e.what());
    }
}

// ============================================================================
// JSFunction JNI
// ============================================================================

JNIEXPORT jobject JNICALL
Java_com_hermes_wrapper_JSFunction_nativeCall(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong funcHandle, jobjectArray jArgs) {
    GET_CTX_RT(ctxHandle, nullptr);
    Object* obj = data->getObject(funcHandle);
    if (!obj || !obj->isFunction(rt)) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), "Not a function");
        return nullptr;
    }

    try {
        Function func = obj->getFunction(rt);
        size_t argc = jArgs ? env->GetArrayLength(jArgs) : 0;
        std::vector<Value> args;
        args.reserve(argc);
        for (size_t i = 0; i < argc; i++) {
            jobject jArg = env->GetObjectArrayElement(jArgs, i);
            args.push_back(javaToValue(env, rt, jArg, data));
            if (jArg) env->DeleteLocalRef(jArg);
        }

        Value result = func.call(rt, static_cast<const Value*>(args.data()), argc);
        return valueToJava(env, rt, result, data);
    } catch (const JSError &e) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"),
                      (std::string("JSError: ") + e.getMessage()).c_str());
        return nullptr;
    } catch (const std::exception &e) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
        return nullptr;
    }
}

JNIEXPORT void JNICALL
Java_com_hermes_wrapper_JSFunction_nativeCallVoid(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong funcHandle, jobjectArray jArgs) {
    GET_CTX_RT(ctxHandle, );
    Object* obj = data->getObject(funcHandle);
    if (!obj || !obj->isFunction(rt)) return;

    try {
        Function func = obj->getFunction(rt);
        size_t argc = jArgs ? env->GetArrayLength(jArgs) : 0;
        std::vector<Value> args;
        args.reserve(argc);
        for (size_t i = 0; i < argc; i++) {
            jobject jArg = env->GetObjectArrayElement(jArgs, i);
            args.push_back(javaToValue(env, rt, jArg, data));
            if (jArg) env->DeleteLocalRef(jArg);
        }
        func.call(rt, static_cast<const Value*>(args.data()), argc);
    } catch (const std::exception &e) {
        LOGE("callVoid error: %s", e.what());
    }
}

} // extern "C"
