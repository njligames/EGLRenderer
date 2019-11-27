#include <jni.h>
#include <string>

#include <strings.h>
#include <android/log.h>

#include <android/native_window.h> // requires ndk r5 or newer
#include <android/native_window_jni.h> // requires ndk r5 or newer
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include "Renderer.h"

#include <errno.h>
#include "stb_image.h"

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

static AAssetManager* asset_manager = nullptr;

static int android_read(void *cookie, char *buf, int size)
{
    return AAsset_read((AAsset*)cookie, buf, size);
}

static int android_write(void *cookie, const char *buf, int size)
{
    return EACCES;
}

static fpos_t android_seek(void *cookie, fpos_t offset, int whence)
{
    return AAsset_seek((AAsset*)cookie, offset, whence);
}

static int android_close(void *cookie)
{
    AAsset_close((AAsset*)cookie);
    return 0;
}

FILE *android_fopen(const char *fname, const char *mode)
{
    AAsset *asset = AAssetManager_open(asset_manager, fname, 0);
    if(!asset)
        return NULL;

    return funopen(asset, android_read, android_write, android_seek, android_close);
}


extern "C" JNIEXPORT void JNICALL
Java_com_example_eglrenderer_MainActivity_init_1asset_1manager(
        JNIEnv* jenv, jclass jclazz, jobject java_asset_manager) {
//    UNUSED(jclazz);
    asset_manager = AAssetManager_fromJava(jenv, java_asset_manager);
}

////
//FileData get_asset_data(const char* relative_path) {
//    assert(relative_path != NULL);
//    AAsset* asset =
//            AAssetManager_open(asset_manager, relative_path, AASSET_MODE_STREAMING);
//    assert(asset != NULL);
//
//    return (FileData) { AAsset_getLength(asset), AAsset_getBuffer(asset), asset };
//}
//
//void release_asset_data(const FileData* file_data) {
//    assert(file_data != NULL);
//    assert(file_data->file_handle != NULL);
//    AAsset_close((AAsset*)file_data->file_handle);
//}