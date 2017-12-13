#include <android/log.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <jni.h>

#define LOG_TAG "ThomasKing"
#define LOGD(fmt, args...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, fmt, ##args)

void init() __attribute__((constructor));

void init(){
	LOGD("Init test!");
}

void test(){
	LOGD("so link test!");
}

static jstring getString(JNIEnv *env, jobject obj){
	return (*env) -> NewStringUTF(env, "Native String Return");
}

// com.loopher.loader   com/example/memloadertest/MainActivity
#define JNIREG_CLASS "com/loopher/loader/JNIHelper"

static JNINativeMethod gMethods[] = {
    {"getString", "()Ljava/lang/String;", (void*)getString}
};

static int registerNativeMethods(JNIEnv* env, const char* className,
        JNINativeMethod* gMethods, int numMethods)
{
	jclass clazz;
	clazz = (*env)->FindClass(env, className);
	if (clazz == NULL) {
		return JNI_FALSE;
	}
	if ((*env)->RegisterNatives(env, clazz, gMethods, numMethods) < 0) {
		return JNI_FALSE;
	}
	return JNI_TRUE;
}

static int registerNatives(JNIEnv* env)
{
	if (!registerNativeMethods(env, JNIREG_CLASS, gMethods,
                                 sizeof(gMethods) / sizeof(gMethods[0])))
		return JNI_FALSE;

	return JNI_TRUE;
}

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved)
{
	JNIEnv* env = NULL;
	if ((*vm)->GetEnv(vm, (void**) &env, JNI_VERSION_1_4) != JNI_OK) {
		return -1;
	}
	if(env == NULL)
		return -1;
	if (!registerNatives(env)) {
		return -1;
	}
	return JNI_VERSION_1_4;
}
