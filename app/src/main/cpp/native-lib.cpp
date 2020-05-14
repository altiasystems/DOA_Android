#include <jni.h>
#include <string>
#include <android/log.h>
#include <unistd.h>
#include <pthread.h>
#include <sstream>

#include "doa.h"

#define LOG_TAG "Native"
static JavaVM * cachedVM = NULL; // updated by JNI_OnLoad
static pthread_key_t jnienv_key;

#define LOGE(FMT, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "[%d*%s:%d:%s]:" FMT,	\
         	 	 	 	 	 	 gettid(), basename(__FILE__), __LINE__, __FUNCTION__, ## __VA_ARGS__)

#define LOGD(FMT, ...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "[%d*%s:%d:%s]:" FMT,	\
         	 	 	 	 	 	 gettid(), basename(__FILE__), __LINE__, __FUNCTION__, ## __VA_ARGS__)

extern "C" JNIEXPORT jstring JNICALL
Java_com_jabra_doa_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}



static JNIEnv * attachThread(const char *threadName)
{
    if (cachedVM == NULL) {
        LOGE("called when cachedVM is NULL VM");
        return NULL;
    }

    JNIEnv * threadEnv = (JNIEnv*) pthread_getspecific(jnienv_key);
    if (threadEnv == NULL) { // if we have already been here from the current thread, this would be non-NULL
        JavaVMAttachArgs thread_spec = {
                JNI_VERSION_1_6, threadName, NULL
        };

        jint ret = cachedVM->AttachCurrentThread(&threadEnv, &thread_spec);
        if (ret || threadEnv == NULL) {
            LOGE("attach to jvm failed");
            return NULL;
        }
        pthread_setspecific(jnienv_key, threadEnv); // cache into thread specific
    }
    return threadEnv;
}

// this gets called whenever a thread dies
static void _android_key_cleanup(void *data)
{
    if (cachedVM == NULL) return;
    JNIEnv* env = (JNIEnv*)pthread_getspecific(jnienv_key);
    if (env != NULL) {
        LOGD("Thread end, detaching jvm from current thread");
        cachedVM->DetachCurrentThread();
        pthread_setspecific(jnienv_key, NULL); // clear the cached key
    }
}

static void
describeException(JNIEnv * env, jthrowable e)
{
    jmethodID toString = env->GetMethodID(env->FindClass("java/lang/Object"), "toString", "()Ljava/lang/String;");
    jstring estring = (jstring) env->CallObjectMethod(e, toString);
    jthrowable ee = env->ExceptionOccurred();
    if (ee) {
        LOGD("jni: exception in describeException\n");
        env->ExceptionClear();
        return;
    }
    jboolean isCopy;
    const char * message = env->GetStringUTFChars(estring, &isCopy);
    LOGD("jni: exception caught %s\n", message);
    env->ReleaseStringUTFChars(estring, message);
}

static std::string getStringFromJ(JNIEnv *env, jstring str)
{
    const char *_str = env->GetStringUTFChars(str, NULL);
    std::string sstr = _str;
    env->ReleaseStringUTFChars(str, _str);
    return sstr;

}

// nativeLibPath can be got from java(android) by accessing: application.getApplicationInfo().nativeLibraryDir
static bool setAdspLibraryPath(std::string nativeLibPath)
{
    std::stringstream path;
    path << nativeLibPath << ";/system/lib/rfsa/adsp;/system/vendor/lib/rfsa/adsp;/dsp";
    return setenv("ADSP_LIBRARY_PATH", path.str().c_str(), 1 /*override*/) == 0;
}


static jlong
startDOA(JNIEnv *env,
        jobject thiz,
        jstring nativeLibDir,
        jstring runTime,
        jstring dlcFile,
        jstring inputFile,
        jstring cacheDir)
{
    std::string snativeLibDir = getStringFromJ(env, nativeLibDir);

    std::string srunTime = getStringFromJ(env, runTime);

    std::string sdlcFile = getStringFromJ(env, dlcFile);

    std::string sinputFile = getStringFromJ(env, inputFile);

    std::string scacheDir = getStringFromJ(env, cacheDir);

    zdl::DlSystem::Runtime_t runtimeTarget;
    if (srunTime == "DSP") runtimeTarget = zdl::DlSystem::Runtime_t::DSP;
    else if (srunTime == "GPU") runtimeTarget = zdl::DlSystem::Runtime_t::GPU;
    else runtimeTarget = zdl::DlSystem::Runtime_t::CPU;

    // change the working directory to the cachedir
    chdir(scacheDir.c_str());

    setAdspLibraryPath(snativeLibDir);

    DOA *doa = new DOA(sdlcFile, sinputFile, ".", runtimeTarget);
    doa->startDOA();

    return (jlong)doa;

}

static void
stopDOA(JNIEnv *env,
        jobject thiz,
        jlong obj)
{
    DOA * doa = reinterpret_cast<DOA *>(obj);
    doa->stopDOA();

}

typedef struct {
    const char* name;
    const char* signature;
} JavaMethod;

static const char *DOAClassName = "com/jabra/doa/MainActivity";

static jmethodID java_get_method(JNIEnv *env, jclass _class, JavaMethod method)
{
    return env->GetMethodID(_class, method.name, method.signature);
}

static JNINativeMethod DOAMethods[] = {
        {"startDOA", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)J", (void *) startDOA},
        {"stopDOA", "(J)V", (void *) stopDOA}
};

static
jint registerNativeMethods(JNIEnv* env, const char *class_name, JNINativeMethod *methods, int num_methods) {
    int result = 0;

    jclass clazz = env->FindClass(class_name);
    if (clazz) {
        int result = env->RegisterNatives(clazz, methods, num_methods);
        if (result < 0) {
            LOGE("registerNativeMethods failed(class=%s)", class_name);
        }
    } else {
        LOGE("registerNativeMethods: class'%s' not found", class_name);
    }
    return result;
}


#define	NUM_ARRAY_ELEMENTS(p)	((int) sizeof(p) / sizeof(p[0]))

static int register_Natives(JNIEnv *env) {
    LOGD("register_Natives:");
    if (registerNativeMethods(env,
            DOAClassName,
            DOAMethods,
            NUM_ARRAY_ELEMENTS(DOAMethods)) < 0) {
        return -1;
    }
    return 0;
}

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {

    LOGD("JNI_OnLoad");

    JNIEnv *env;
    if (vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    int result = register_Natives(env);

    cachedVM = vm;
    pthread_key_create(&jnienv_key, _android_key_cleanup);

    LOGD("JNI_OnLoad:finshed:result=%d", result);
    return JNI_VERSION_1_6;
}
