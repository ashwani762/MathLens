// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Ashwani

// JNI bridge between Kotlin (com.mathlens.app.engine.MathEngine) and the portable C++ engine.
// All heavy lifting lives in ../../../../../mathengine; this file only converts types.
#include <jni.h>

#include <string>

#include "mathengine/mathengine.h"

namespace {

std::string toStd(JNIEnv* env, jstring s) {
    if (!s) return {};
    const char* utf = env->GetStringUTFChars(s, nullptr);
    std::string out(utf ? utf : "");
    if (utf) env->ReleaseStringUTFChars(s, utf);
    return out;
}

// NewStringUTF expects "modified UTF-8"; build the string from UTF-16 instead so that
// characters outside the BMP (rare in LaTeX, but possible) never break the conversion.
jstring toJava(JNIEnv* env, const std::string& s) {
    std::u16string u16;
    u16.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        char32_t cp;
        int len;
        if (c < 0x80) { cp = c; len = 1; }
        else if ((c >> 5) == 0x6) { cp = c & 0x1F; len = 2; }
        else if ((c >> 4) == 0xE) { cp = c & 0x0F; len = 3; }
        else { cp = c & 0x07; len = 4; }
        for (int k = 1; k < len && i + k < s.size(); ++k) cp = (cp << 6) | ((unsigned char)s[i + k] & 0x3F);
        i += len;
        if (cp >= 0x10000) {
            cp -= 0x10000;
            u16.push_back((char16_t)(0xD800 + (cp >> 10)));
            u16.push_back((char16_t)(0xDC00 + (cp & 0x3FF)));
        } else u16.push_back((char16_t)cp);
    }
    return env->NewString(reinterpret_cast<const jchar*>(u16.data()), (jsize)u16.size());
}

}  // namespace

extern "C" {

JNIEXPORT jstring JNICALL Java_com_mathlens_app_engine_MathEngine_nativeSolve(JNIEnv* env, jobject, jstring input) {
    return toJava(env, me::solveJson(toStd(env, input)));
}

JNIEXPORT jstring JNICALL Java_com_mathlens_app_engine_MathEngine_nativeLatex(JNIEnv* env, jobject, jstring input) {
    return toJava(env, me::latexJson(toStd(env, input)));
}

JNIEXPORT jstring JNICALL Java_com_mathlens_app_engine_MathEngine_nativeFromLatex(JNIEnv* env, jobject, jstring latex) {
    char* out = me_from_latex(toStd(env, latex).c_str());
    std::string s = out ? out : "";
    me_free(out);
    return toJava(env, s);
}

JNIEXPORT jstring JNICALL Java_com_mathlens_app_engine_MathEngine_nativeGraph(JNIEnv* env, jobject, jstring input) {
    return toJava(env, me::graphJson(toStd(env, input)));
}

JNIEXPORT jlong JNICALL Java_com_mathlens_app_engine_MathEngine_nativeCompile(JNIEnv* env, jobject, jstring expr) {
    return reinterpret_cast<jlong>(me_compile(toStd(env, expr).c_str()));
}

JNIEXPORT void JNICALL Java_com_mathlens_app_engine_MathEngine_nativeRelease(JNIEnv*, jobject, jlong handle) {
    me_program_free(reinterpret_cast<me_program*>(handle));
}

JNIEXPORT jboolean JNICALL Java_com_mathlens_app_engine_MathEngine_nativeUsesY(JNIEnv*, jobject, jlong handle) {
    return me_uses_y(reinterpret_cast<me_program*>(handle)) ? JNI_TRUE : JNI_FALSE;
}

// Evaluates the program at n points. ys may be null. Uses critical arrays (no copies) since
// evaluation is short and never calls back into Java.
JNIEXPORT void JNICALL Java_com_mathlens_app_engine_MathEngine_nativeEvalMany(JNIEnv* env, jobject, jlong handle, jdoubleArray xs,
                                                                              jdoubleArray ys, jdoubleArray out) {
    auto* p = reinterpret_cast<me_program*>(handle);
    if (!p || !xs || !out) return;
    jsize n = env->GetArrayLength(out);
    if (env->GetArrayLength(xs) < n) n = env->GetArrayLength(xs);
    if (ys && env->GetArrayLength(ys) < n) n = env->GetArrayLength(ys);
    auto* px = static_cast<double*>(env->GetPrimitiveArrayCritical(xs, nullptr));
    auto* py = ys ? static_cast<double*>(env->GetPrimitiveArrayCritical(ys, nullptr)) : nullptr;
    auto* po = static_cast<double*>(env->GetPrimitiveArrayCritical(out, nullptr));
    if (px && po) me_eval_many(p, px, py, (int)n, po);
    if (po) env->ReleasePrimitiveArrayCritical(out, po, 0);
    if (py) env->ReleasePrimitiveArrayCritical(ys, py, JNI_ABORT);
    if (px) env->ReleasePrimitiveArrayCritical(xs, px, JNI_ABORT);
}

JNIEXPORT jdouble JNICALL Java_com_mathlens_app_engine_MathEngine_nativeEval(JNIEnv*, jobject, jlong handle, jdouble x, jdouble y, jdouble t) {
    return me_eval(reinterpret_cast<me_program*>(handle), x, y, t);
}

}  // extern "C"
