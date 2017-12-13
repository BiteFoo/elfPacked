#include <jni.h>
#include <string>
#include "mylinker.h"
#define SO_PATH    "/data/data/com.loopher.loader/files/libfoo.so"   //elf_loader.so// /data/local/tmp/libMini_elf_loader.so


// /data/data/com.loopher.loader/files/elf_loader.so
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

static unsigned elfhash(const char *_name){
    const unsigned char *name = (const unsigned char *) _name;
    unsigned h = 0, g = 0;

    while(*name) {
        h = (h << 4) + *name++;
        g = h & 0xf0000000;
        h ^= g;
        h ^= g >> 24;
    }
    return h;
}

static unsigned findSym(soinfo *si, const char* name){
    unsigned i, hashval;
    Elf32_Sym *symtab = si->symtab;
    const char *strtab = si->strtab;
    unsigned nbucket = si->nbucket;
    unsigned *bucket = si->bucket;
    unsigned *chain  = si->chain;

    hashval = elfhash(name);
    for(i = bucket[hashval % nbucket]; i != 0; i = chain[i]){
        if(symtab[i].st_shndx != 0){
            if(strcmp(strtab + symtab[i].st_name, name) == 0){
                return symtab[i].st_value;
            }
        }
    }
    return 0;
}


static void* load(){
    jint (*real_JNI_OnLoad)(JavaVM *vm, void *);
    void* jni_onload=NULL;
    const char* so_name = SO_PATH;
    int fd  = open_library_on_path(so_name);
    soinfo* si =load_so(so_name,fd);//装载so
    if (si == NULL){
        __android_log_print(ANDROID_LOG_INFO,"Loopher","open file was failed ");
    }
    else{
        DL_INFO("完成装载so，准备做链接操作");
        if(!linke_so_img(si))//连接so
        {
            DL_ERR(" \"%s\" linked failed ",so_name);
        }
        DL_INFO("完成装载链接工作");
//        real_JNI_OnLoad = (jint (*)(JavaVM* ,void*))(si->base+findSym(si,"JNI_OnLoad"));
        jni_onload= (void *) (si->base + findSym(si, "JNI_OnLoad"));
        if(jni_onload == NULL){
            DL_ERR("real_JNI_OnLoad 失败*******************》》》");
            return NULL;
        }
        else{
            DL_INFO("成功****************》》》 real_JNI_OnLoad addr = %p",jni_onload);
        }
    }
    close(fd);
    return jni_onload;
}




static jstring  load_and_link_elf(JNIEnv *env){

    jint (*real_JNI_OnLoad) (JavaVM *vm,void*);
    const char* so_name = SO_PATH;
    std::string hello = "Hello from C++";
    int fd  = open_library_on_path(so_name);
    soinfo* si =load_so(so_name,fd);//装载so
    if (si == NULL){
         hello = "load so failed";
        __android_log_print(ANDROID_LOG_INFO,"Loopher","open file was failed ");
    }
    else{
         hello = "完成装载so，准备做链接操作";
          DL_INFO("完成装载so，准备做链接操作");
        if(!linke_so_img(si))//连接so
        {
            DL_ERR(" \"%s\" linked failed ",so_name);
        }
        DL_INFO("完成装载链接工作");
        real_JNI_OnLoad = (jint (*)(JavaVM* ,void*))(si->base+findSym(si,"JNI_OnLoad"));
        if(real_JNI_OnLoad == NULL){
            DL_ERR("real_JNI_OnLoad 失败*******************》》》");
        }
        else{
            DL_INFO("成功****************》》》 real_JNI_OnLoad addr = %p",real_JNI_OnLoad);
        }
    }
    close(fd);
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

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved){

    DL_INFO("call ========== JNI_OnLoad ");
    jint (*real_JNI_OnLoad)(JavaVM*, void*);
    real_JNI_OnLoad = (jint (*)(JavaVM *, void *)) load();
    if(real_JNI_OnLoad== NULL){
        DL_ERR("loadMini_library failed\n");
        return JNI_FALSE;
    }
    if(real_JNI_OnLoad != NULL){
        DL_ERR(" find sym %s  addr = %p\n", "JNI_OnLoad",real_JNI_OnLoad);
    }
    return real_JNI_OnLoad(vm, reserved);
}


