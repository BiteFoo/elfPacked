#include <jni.h>
#include <string>
#include "mylinker.h"
#define SO_PATH "/data/local/tmp/mydir/libMini_elf_loader.so"

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

static jstring  load_and_link_elf(JNIEnv *env){
    const char* so_name = SO_PATH;
    std::string hello = "Hello from C++";
    int fd  = open_library_on_path(so_name);
    soinfo* si =load_so(so_name,fd);//装载so
    if (si == NULL){
        std::string hello = "load so failed";
//        return env->NewStringUTF(hello.c_str());
    }
    else{
        std::string hello = "完成装载so，准备做链接操作";
        DL_INFO("完成装载so，准备做链接操作");
        if(!linke_so_img(si))//连接so
        {
            DL_ERR(" \"%s\" linked failed ",so_name);
        }

        DL_INFO("完成装载链接工作");
    }
    close(fd);
//    if(si != NULL){ //这里释放掉资
//        free(si);
//        si =NULL;
//    }
    return env->NewStringUTF(hello.c_str());

}


extern "C"
 JNIEXPORT jstring

JNICALL
Java_com_loopher_loader_MainActivity_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {

    return load_and_link_elf(env);
}
