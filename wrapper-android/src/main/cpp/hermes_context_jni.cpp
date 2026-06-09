#include <jni.h>
#include <android/log.h>
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>

#include <hermes/hermes.h>
#include <jsi/jsi.h>

#define TAG "HermesWrapper"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

using namespace facebook::jsi;
using namespace facebook::hermes;

// ============================================================================
// Object Store: maps integer handles to JSI Object pointers
// This lets us pass JS objects across JNI as long handles.
// ============================================================================

struct HermesContextData {
    std::unique_ptr<HermesRuntime> runtime;
    std::unordered_map<long, std::shared_ptr<Object>> objectStore;
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
        if (it != objectStore.end()) return it->second.get();
        return nullptr;
    }

    void releaseObject(long handle) {
        std::lock_guard<std::mutex> lock(mtx);
        objectStore.erase(handle);
    }
};

// ============================================================================
// Helpers
// ============================================================================

static JavaVM* g_jvm = nullptr;

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
    g_jvm = vm;
    return JNI_VERSION_1_6;
}

static JNIEnv* getEnv() {
    JNIEnv* env = nullptr;
    if (g_jvm) g_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    return env;
}

static std::string jstringToString(JNIEnv* env, jstring str) {
    if (!str) return "";
    const char* chars = env->GetStringUTFChars(str, nullptr);
    std::string result(chars);
    env->ReleaseStringUTFChars(str, chars);
    return result;
}

// Convert a JSI Value to a Java Object
static jobject valueToJava(JNIEnv* env, Runtime& rt, const Value& val, HermesContextData* ctx) {
    if (val.isNull() || val.isUndefined()) {
        return nullptr;
    } else if (val.isBool()) {
        jclass cls = env->FindClass("java/lang/Boolean");
        jmethodID mid = env->GetStaticMethodID(cls, "valueOf", "(Z)Ljava/lang/Boolean;");
        return env->CallStaticObjectMethod(cls, mid, val.getBool() ? JNI_TRUE : JNI_FALSE);
    } else if (val.isNumber()) {
        jclass cls = env->FindClass("java/lang/Double");
        jmethodID mid = env->GetStaticMethodID(cls, "valueOf", "(D)Ljava/lang/Double;");
        return env->CallStaticObjectMethod(cls, mid, val.getNumber());
    } else if (val.isString()) {
        std::string s = val.getString(rt).utf8(rt);
        return env->NewStringUTF(s.c_str());
    } else if (val.isObject()) {
        Object obj = val.getObject(rt);
        long handle = ctx->storeObject(std::move(obj));
        // Determine if it's an array or function
        Object* stored = ctx->getObject(handle);
        if (stored->isArray(rt)) {
            jclass cls = env->FindClass("com/hermes/wrapper/JSArray");
            jmethodID mid = env->GetMethodID(cls, "<init>", "(JJ)V");
            return env->NewObject(cls, mid, reinterpret_cast<jlong>(ctx), (jlong)handle);
        } else if (stored->isFunction(rt)) {
            jclass cls = env->FindClass("com/hermes/wrapper/JSFunction");
            jmethodID mid = env->GetMethodID(cls, "<init>", "(JJ)V");
            return env->NewObject(cls, mid, reinterpret_cast<jlong>(ctx), (jlong)handle);
        } else {
            jclass cls = env->FindClass("com/hermes/wrapper/JSObject");
            jmethodID mid = env->GetMethodID(cls, "<init>", "(JJ)V");
            return env->NewObject(cls, mid, reinterpret_cast<jlong>(ctx), (jlong)handle);
        }
    }
    return nullptr;
}

// Convert a Java Object to a JSI Value
static Value javaToValue(JNIEnv* env, Runtime& rt, jobject obj, HermesContextData* ctx) {
    if (!obj) return Value::null();

    jclass stringCls = env->FindClass("java/lang/String");
    jclass intCls = env->FindClass("java/lang/Integer");
    jclass longCls = env->FindClass("java/lang/Long");
    jclass doubleCls = env->FindClass("java/lang/Double");
    jclass floatCls = env->FindClass("java/lang/Float");
    jclass boolCls = env->FindClass("java/lang/Boolean");
    jclass jsObjCls = env->FindClass("com/hermes/wrapper/JSObject");

    if (env->IsInstanceOf(obj, stringCls)) {
        std::string s = jstringToString(env, (jstring)obj);
        return String::createFromUtf8(rt, s);
    } else if (env->IsInstanceOf(obj, intCls)) {
        jmethodID mid = env->GetMethodID(intCls, "intValue", "()I");
        return Value((double)env->CallIntMethod(obj, mid));
    } else if (env->IsInstanceOf(obj, longCls)) {
        jmethodID mid = env->GetMethodID(longCls, "longValue", "()J");
        return Value((double)env->CallLongMethod(obj, mid));
    } else if (env->IsInstanceOf(obj, doubleCls)) {
        jmethodID mid = env->GetMethodID(doubleCls, "doubleValue", "()D");
        return Value(env->CallDoubleMethod(obj, mid));
    } else if (env->IsInstanceOf(obj, floatCls)) {
        jmethodID mid = env->GetMethodID(floatCls, "floatValue", "()F");
        return Value((double)env->CallFloatMethod(obj, mid));
    } else if (env->IsInstanceOf(obj, boolCls)) {
        jmethodID mid = env->GetMethodID(boolCls, "booleanValue", "()Z");
        return Value(env->CallBooleanMethod(obj, mid) == JNI_TRUE);
    } else if (env->IsInstanceOf(obj, jsObjCls)) {
        jfieldID fid = env->GetFieldID(jsObjCls, "nativeHandle", "J");
        long handle = (long)env->GetLongField(obj, fid);
        Object* stored = ctx->getObject(handle);
        if (stored) {
            return Value(rt, *stored);
        }
    }

    // Check if it's a JSCallFunction (Java callback → JS function)
    jclass callFnCls = env->FindClass("com/hermes/wrapper/JSCallFunction");
    if (env->IsInstanceOf(obj, callFnCls)) {
        jobject callbackRef = env->NewGlobalRef(obj);
        auto hostFn = Function::createFromHostFunction(rt,
            PropNameID::forUtf8(rt, "javaCallback"), 10,
            [ctx, callbackRef](Runtime& rt, const Value& thisVal,
                               const Value* args, size_t count) -> Value {
                JNIEnv* env = getEnv();
                if (!env) return Value::undefined();

                jclass objCls = env->FindClass("java/lang/Object");
                jobjectArray jArgs = env->NewObjectArray(count, objCls, nullptr);
                for (size_t i = 0; i < count; i++) {
                    jobject jArg = valueToJava(env, rt, args[i], ctx);
                    env->SetObjectArrayElement(jArgs, i, jArg);
                    if (jArg) env->DeleteLocalRef(jArg);
                }

                jclass fnCls = env->FindClass("com/hermes/wrapper/JSCallFunction");
                jmethodID callMid = env->GetMethodID(fnCls, "call",
                    "([Ljava/lang/Object;)Ljava/lang/Object;");
                jobject jResult = env->CallObjectMethod(callbackRef, callMid, jArgs);

                Value result = javaToValue(env, rt, jResult, ctx);
                env->DeleteLocalRef(jArgs);
                if (jResult) env->DeleteLocalRef(jResult);
                return result;
            });
        return Value(rt, std::move(hostFn));
    }

    return Value::null();
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
        LOGI("Destroying Hermes runtime (releasing %zu objects)", data->objectStore.size());
        delete data;
    }
}

JNIEXPORT jlong JNICALL
Java_com_hermes_wrapper_HermesContext_nativeGetGlobalObject(JNIEnv *env, jobject thiz, jlong handle) {
    auto data = reinterpret_cast<HermesContextData *>(handle);
    if (!data || !data->runtime) return 0;
    auto &rt = *data->runtime;
    Object global = rt.global();
    return data->storeObject(std::move(global));
}

JNIEXPORT jlong JNICALL
Java_com_hermes_wrapper_HermesContext_nativeCreateObject(JNIEnv *env, jobject thiz, jlong handle) {
    auto data = reinterpret_cast<HermesContextData *>(handle);
    if (!data || !data->runtime) return 0;
    auto &rt = *data->runtime;
    Object obj(rt);
    return data->storeObject(std::move(obj));
}

JNIEXPORT jlong JNICALL
Java_com_hermes_wrapper_HermesContext_nativeCreateArray(JNIEnv *env, jobject thiz, jlong handle) {
    auto data = reinterpret_cast<HermesContextData *>(handle);
    if (!data || !data->runtime) return 0;
    auto &rt = *data->runtime;
    Array arr = Array(rt, 0);
    return data->storeObject(std::move(arr));
}

JNIEXPORT jstring JNICALL
Java_com_hermes_wrapper_HermesContext_nativeEval(JNIEnv *env, jobject thiz, jlong handle, jstring code) {
    auto data = reinterpret_cast<HermesContextData *>(handle);
    if (!data || !data->runtime) {
        env->ThrowNew(env->FindClass("java/lang/IllegalStateException"), "Runtime is null");
        return nullptr;
    }

    std::string codeString = jstringToString(env, code);

    try {
        auto &rt = *data->runtime;
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
            if (num == (int64_t) num && num >= -1e15 && num <= 1e15) {
                resultStr = std::to_string((int64_t) num);
            } else {
                resultStr = std::to_string(num);
            }
        } else if (result.isString()) {
            resultStr = result.getString(rt).utf8(rt);
        } else if (result.isObject()) {
            auto JSON = rt.global().getPropertyAsObject(rt, "JSON");
            auto stringify = JSON.getPropertyAsFunction(rt, "stringify");
            Value jsonResult = stringify.call(rt, result);
            if (jsonResult.isString()) {
                resultStr = jsonResult.getString(rt).utf8(rt);
            } else {
                resultStr = "[object]";
            }
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
    auto data = reinterpret_cast<HermesContextData *>(handle);
    if (!data || !data->runtime) return JNI_FALSE;
    std::string codeString = jstringToString(env, code);
    try {
        auto &rt = *data->runtime;
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
    auto data = reinterpret_cast<HermesContextData *>(handle);
    if (!data || !data->runtime) return 0.0;
    std::string codeString = jstringToString(env, code);
    try {
        auto &rt = *data->runtime;
        Value result = rt.evaluateJavaScript(std::make_shared<StringBuffer>(codeString), "<eval>");
        if (result.isNumber()) return result.getNumber();
        return 0.0;
    } catch (...) {
        return 0.0;
    }
}

JNIEXPORT jint JNICALL
Java_com_hermes_wrapper_HermesContext_nativeEvalInt(JNIEnv *env, jobject thiz, jlong handle, jstring code) {
    auto data = reinterpret_cast<HermesContextData *>(handle);
    if (!data || !data->runtime) return 0;
    std::string codeString = jstringToString(env, code);
    try {
        auto &rt = *data->runtime;
        Value result = rt.evaluateJavaScript(std::make_shared<StringBuffer>(codeString), "<eval>");
        if (result.isNumber()) return (jint) result.getNumber();
        return 0;
    } catch (...) {
        return 0;
    }
}

JNIEXPORT void JNICALL
Java_com_hermes_wrapper_HermesContext_nativeRunScript(JNIEnv *env, jobject thiz, jlong handle, jstring script) {
    auto data = reinterpret_cast<HermesContextData *>(handle);
    if (!data || !data->runtime) return;
    std::string scriptString = jstringToString(env, script);
    try {
        auto &rt = *data->runtime;
        rt.evaluateJavaScript(std::make_shared<StringBuffer>(scriptString), "<script>");
    } catch (const JSError &e) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"),
                      (std::string("JSError: ") + e.getMessage()).c_str());
    } catch (const std::exception &e) {
        env->ThrowNew(env->FindClass("java/lang/RuntimeException"), e.what());
    }
}

// ============================================================================
// JSObject JNI
// ============================================================================

JNIEXPORT void JNICALL
Java_com_hermes_wrapper_JSObject_nativeSetPropertyString(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong objHandle, jstring name, jstring value) {
    auto data = reinterpret_cast<HermesContextData *>(ctxHandle);
    if (!data || !data->runtime) return;
    auto &rt = *data->runtime;
    Object* obj = data->getObject(objHandle);
    if (!obj) return;

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
    auto data = reinterpret_cast<HermesContextData *>(ctxHandle);
    if (!data || !data->runtime) return;
    auto &rt = *data->runtime;
    Object* obj = data->getObject(objHandle);
    if (!obj) return;

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
    auto data = reinterpret_cast<HermesContextData *>(ctxHandle);
    if (!data || !data->runtime) return;
    auto &rt = *data->runtime;
    Object* obj = data->getObject(objHandle);
    if (!obj) return;

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
    auto data = reinterpret_cast<HermesContextData *>(ctxHandle);
    if (!data || !data->runtime) return;
    auto &rt = *data->runtime;
    Object* obj = data->getObject(objHandle);
    if (!obj) return;

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
    auto data = reinterpret_cast<HermesContextData *>(ctxHandle);
    if (!data || !data->runtime) return;
    auto &rt = *data->runtime;
    Object* obj = data->getObject(objHandle);
    Object* valObj = data->getObject(valueHandle);
    if (!obj || !valObj) return;

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
    auto data = reinterpret_cast<HermesContextData *>(ctxHandle);
    if (!data || !data->runtime) return;
    auto &rt = *data->runtime;
    Object* obj = data->getObject(objHandle);
    if (!obj) return;

    std::string nameStr = jstringToString(env, name);

    // Create a persistent global ref to the Java callback
    jobject callbackRef = env->NewGlobalRef(javaCallback);

    // Create a host function that calls back to Java
    auto hostFn = Function::createFromHostFunction(rt,
        PropNameID::forUtf8(rt, nameStr), 10, // max 10 params
        [data, callbackRef](Runtime& rt, const Value& thisVal,
                           const Value* args, size_t count) -> Value {
            JNIEnv* env = getEnv();
            if (!env) return Value::undefined();

            // Convert JS args to Java Object[]
            jclass objCls = env->FindClass("java/lang/Object");
            jobjectArray jArgs = env->NewObjectArray(count, objCls, nullptr);
            for (size_t i = 0; i < count; i++) {
                jobject jArg = valueToJava(env, rt, args[i], data);
                env->SetObjectArrayElement(jArgs, i, jArg);
                if (jArg) env->DeleteLocalRef(jArg);
            }

            // Call Java: JSCallFunction.call(Object... args) -> Object
            jclass callFnCls = env->FindClass("com/hermes/wrapper/JSCallFunction");
            jmethodID callMid = env->GetMethodID(callFnCls, "call",
                "([Ljava/lang/Object;)Ljava/lang/Object;");
            jobject jResult = env->CallObjectMethod(callbackRef, callMid, jArgs);

            // Convert result back to JSI Value
            Value result = javaToValue(env, rt, jResult, data);

            env->DeleteLocalRef(jArgs);
            if (jResult) env->DeleteLocalRef(jResult);
            return result;
        });

    try {
        obj->setProperty(rt, PropNameID::forUtf8(rt, nameStr), std::move(hostFn));
    } catch (const std::exception &e) {
        LOGE("setPropertyFunction error: %s", e.what());
        env->DeleteGlobalRef(callbackRef);
    }
}

// --- Get Property ---

JNIEXPORT jstring JNICALL
Java_com_hermes_wrapper_JSObject_nativeGetPropertyString(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong objHandle, jstring name) {
    auto data = reinterpret_cast<HermesContextData *>(ctxHandle);
    if (!data || !data->runtime) return nullptr;
    auto &rt = *data->runtime;
    Object* obj = data->getObject(objHandle);
    if (!obj) return nullptr;

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
    auto data = reinterpret_cast<HermesContextData *>(ctxHandle);
    if (!data || !data->runtime) return 0;
    auto &rt = *data->runtime;
    Object* obj = data->getObject(objHandle);
    if (!obj) return 0;

    std::string nameStr = jstringToString(env, name);
    try {
        Value val = obj->getProperty(rt, PropNameID::forUtf8(rt, nameStr));
        if (val.isNumber()) return (jint)val.getNumber();
        return 0;
    } catch (...) {
        return 0;
    }
}

JNIEXPORT jdouble JNICALL
Java_com_hermes_wrapper_JSObject_nativeGetPropertyDouble(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong objHandle, jstring name) {
    auto data = reinterpret_cast<HermesContextData *>(ctxHandle);
    if (!data || !data->runtime) return 0.0;
    auto &rt = *data->runtime;
    Object* obj = data->getObject(objHandle);
    if (!obj) return 0.0;

    std::string nameStr = jstringToString(env, name);
    try {
        Value val = obj->getProperty(rt, PropNameID::forUtf8(rt, nameStr));
        if (val.isNumber()) return val.getNumber();
        return 0.0;
    } catch (...) {
        return 0.0;
    }
}

JNIEXPORT jboolean JNICALL
Java_com_hermes_wrapper_JSObject_nativeGetPropertyBoolean(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong objHandle, jstring name) {
    auto data = reinterpret_cast<HermesContextData *>(ctxHandle);
    if (!data || !data->runtime) return JNI_FALSE;
    auto &rt = *data->runtime;
    Object* obj = data->getObject(objHandle);
    if (!obj) return JNI_FALSE;

    std::string nameStr = jstringToString(env, name);
    try {
        Value val = obj->getProperty(rt, PropNameID::forUtf8(rt, nameStr));
        if (val.isBool()) return val.getBool() ? JNI_TRUE : JNI_FALSE;
        return JNI_FALSE;
    } catch (...) {
        return JNI_FALSE;
    }
}

JNIEXPORT jlong JNICALL
Java_com_hermes_wrapper_JSObject_nativeGetPropertyObject(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong objHandle, jstring name) {
    auto data = reinterpret_cast<HermesContextData *>(ctxHandle);
    if (!data || !data->runtime) return 0;
    auto &rt = *data->runtime;
    Object* obj = data->getObject(objHandle);
    if (!obj) return 0;

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
    auto data = reinterpret_cast<HermesContextData *>(ctxHandle);
    if (!data || !data->runtime) return JNI_FALSE;
    auto &rt = *data->runtime;
    Object* obj = data->getObject(objHandle);
    if (!obj) return JNI_FALSE;

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
    if (data) {
        data->releaseObject(objHandle);
    }
}

// ============================================================================
// JSArray JNI
// ============================================================================

JNIEXPORT jint JNICALL
Java_com_hermes_wrapper_JSArray_nativeArrayLength(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong objHandle) {
    auto data = reinterpret_cast<HermesContextData *>(ctxHandle);
    if (!data || !data->runtime) return 0;
    auto &rt = *data->runtime;
    Object* obj = data->getObject(objHandle);
    if (!obj || !obj->isArray(rt)) return 0;

    try {
        Array arr = obj->getArray(rt);
        return (jint)arr.size(rt);
    } catch (...) {
        return 0;
    }
}

JNIEXPORT jobject JNICALL
Java_com_hermes_wrapper_JSArray_nativeArrayGet(JNIEnv *env, jclass clazz,
        jlong ctxHandle, jlong objHandle, jint index) {
    auto data = reinterpret_cast<HermesContextData *>(ctxHandle);
    if (!data || !data->runtime) return nullptr;
    auto &rt = *data->runtime;
    Object* obj = data->getObject(objHandle);
    if (!obj || !obj->isArray(rt)) return nullptr;

    try {
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
    auto data = reinterpret_cast<HermesContextData *>(ctxHandle);
    if (!data || !data->runtime) return;
    auto &rt = *data->runtime;
    Object* obj = data->getObject(objHandle);
    if (!obj || !obj->isArray(rt)) return;

    try {
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
    auto data = reinterpret_cast<HermesContextData *>(ctxHandle);
    if (!data || !data->runtime) return nullptr;
    auto &rt = *data->runtime;
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
    auto data = reinterpret_cast<HermesContextData *>(ctxHandle);
    if (!data || !data->runtime) return;
    auto &rt = *data->runtime;
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
