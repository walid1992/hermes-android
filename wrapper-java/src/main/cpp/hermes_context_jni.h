#pragma once

#include <jni.h>
#include <string>
#include <memory>

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT jlong JNICALL
Java_com_hermes_wrapper_HermesContext_nativeCreate(JNIEnv *env, jobject thiz);

JNIEXPORT void JNICALL
Java_com_hermes_wrapper_HermesContext_nativeDestroy(JNIEnv *env, jobject thiz, jlong handle);

JNIEXPORT jstring JNICALL
Java_com_hermes_wrapper_HermesContext_nativeEval(JNIEnv *env, jobject thiz, jlong handle, jstring code);

JNIEXPORT jboolean JNICALL
Java_com_hermes_wrapper_HermesContext_nativeEvalBoolean(JNIEnv *env, jobject thiz, jlong handle, jstring code);

JNIEXPORT jdouble JNICALL
Java_com_hermes_wrapper_HermesContext_nativeEvalDouble(JNIEnv *env, jobject thiz, jlong handle, jstring code);

JNIEXPORT jint JNICALL
Java_com_hermes_wrapper_HermesContext_nativeEvalInt(JNIEnv *env, jobject thiz, jlong handle, jstring code);

JNIEXPORT void JNICALL
Java_com_hermes_wrapper_HermesContext_nativeSetGlobal(JNIEnv *env, jobject thiz, jlong handle, jstring name, jstring value);

JNIEXPORT jstring JNICALL
Java_com_hermes_wrapper_HermesContext_nativeGetGlobal(JNIEnv *env, jobject thiz, jlong handle, jstring name);

JNIEXPORT void JNICALL
Java_com_hermes_wrapper_HermesContext_nativeRunScript(JNIEnv *env, jobject thiz, jlong handle, jstring script);

JNIEXPORT jstring JNICALL
Java_com_hermes_wrapper_HermesContext_nativeCallFunction(JNIEnv *env, jobject thiz, jlong handle, jstring funcName, jstring argsJson);

#ifdef __cplusplus
}
#endif
