#include <jni.h>
#include <string>

#include <strings.h>
#include <android/log.h>

#include <android/native_window.h> // requires ndk r5 or newer
#include <android/native_window_jni.h> // requires ndk r5 or newer
#include "Renderer.h"

static ANativeWindow *window = 0;
static Renderer *renderer = 0;

#define LOG_TAG "EglSample"

#define LOG_INFO(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOG_ERROR(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_eglrenderer_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_eglrenderer_MainActivity_nativeOnStart(JNIEnv* jenv, jobject obj)
{
    LOG_INFO("nativeOnStart");
    renderer = new Renderer();
}

//public native void nativeOnResume();
extern "C" JNIEXPORT void JNICALL
Java_com_example_eglrenderer_MainActivity_nativeOnResume(JNIEnv* jenv, jobject obj)
{
    LOG_INFO("nativeOnResume");
    renderer->start();
}
//public native void nativeOnPause();
extern "C" JNIEXPORT void JNICALL
Java_com_example_eglrenderer_MainActivity_nativeOnPause(JNIEnv* jenv, jobject obj)
{
    LOG_INFO("nativeOnPause");
    renderer->stop();
}
//public native void nativeOnStop();
extern "C" JNIEXPORT void JNICALL
Java_com_example_eglrenderer_MainActivity_nativeOnStop(JNIEnv* jenv, jobject obj)
{
    LOG_INFO("nativeOnStop");
    delete renderer;
    renderer = 0;
}



extern "C" JNIEXPORT void JNICALL
Java_com_example_eglrenderer_MainActivity_nativeSetSurface(JNIEnv* jenv, jobject obj, jobject surface)
{
    if (surface != 0) {
        window = ANativeWindow_fromSurface(jenv, surface);
        LOG_INFO("Got window %p", window);
        renderer->setWindow(window);
    } else {
        LOG_INFO("Releasing window");
        ANativeWindow_release(window);
    }
}
