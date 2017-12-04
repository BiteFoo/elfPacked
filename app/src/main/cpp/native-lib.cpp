#include <jni.h>
#include <string>
#include <android/log.h>
#include <unistd.h>
#include <fcntl.h>

#include "mylinker.h"
//使用默认的动态库的路径
static const char* const kDefaultLdPaths[] = {
        #if defined(__LP64__)
          "/vendor/lib64",
         "/system/lib64",
     #else
       "/vendor/lib",
        "/system/lib",
       #endif
        NULL
};

/**
 * 打开指定路径的so文件
 * */
static int open_library_on_path(const char* name)
 {
     int fd =-1;
     // If the name contains a slash, we should attempt to open it directly and not search the paths.
     if (strchr(name, '/') != NULL) {
              fd = TEMP_FAILURE_RETRY(open(name, O_RDONLY | O_CLOEXEC));
           if (fd != -1) {
              return fd;
             }
            // ...but nvidia binary blobs (at least) rely on this behavior, so fall through for now.
  #if defined(__LP64__)
          return -1;
       #endif
      }
     fd = TEMP_FAILURE_RETRY(open(name, O_RDONLY | O_CLOEXEC));
     if (fd != -1) {
         return fd;
     }

    return fd;
}


extern "C"
JNIEXPORT jstring

JNICALL
Java_com_loopher_loader_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {

    const char* so = "";

    int a;//没有初始化，是一个随机数
    __android_log_print(ANDROID_LOG_DEBUG,"Loopher","a=%d",a);
    char b ='a'+25;
    __android_log_print(ANDROID_LOG_DEBUG,"Loopher","b=%c",b);
    std::string hello = "Hello from C++";

    int fd  = open_library_on_path(so);
    soinfo* si =load_so(fd);//装载so
    if (si == NULL){
        std::string hello = "load so failed";
        return env->NewStringUTF(hello.c_str());
    }
    else{
        linke_so_img(si);//连接so
    }

    return env->NewStringUTF(hello.c_str());
}
