关于如何编写自定义的linker，查看相关文章和对应的源码
源码：Android5.0
http://androidxref.com/5.0.0_r2/xref/bionic/linker/linker.cpp#open_library_on_path
文章：
关于如何自定义linker并且相关细节
http://blog.csdn.net/hgl868/article/details/52759921
有人写过并且给了源码，没有账号
http://www.pd521.com/thread-455-1-1.html
来自看雪的文章Thomasking的文章。

//fenxi linker
http://www.evil0x.com/posts/15502.html


**************************
2017-12-6
判断Android 的cpu架构
https://github.com/pgyszhh/CPUTypeHelper/blob/master/app/src/main/cpp/CpuFeatures.cpp


******************************
2017-12-6
基本完成了装载功能，目前需要完成链接的功能。
程序使用的测试文件，libMini_elf_loader.so 先测试，后面还要更改这个程序


*****************************************
2017-12-7
在测试的过程中发现，是用malloc分配的内存soinfo*,在最后调用 constructor函数的时候，出现了奔溃，
于是再次查看linker.cpp中的代码，看到如下提示：
/* >>> IMPORTANT NOTE - READ ME BEFORE MODIFYING <<<
 *
 * Do NOT use malloc() and friends or pthread_*() code here.
 * Don't use printf() either; it's caused mysterious memory
 * corruption in the past.
 * The linker runs before we bring up libc and it's easiest
 * to make sure it does not depend on any complex libc features


 提示不要使用malloc函数和友元或者是pthread*等函数调用，也不能使用printf()函数，
 不然会出现未知的错误。

 那么我在分配内存的时候，使用的是
 *********************************************
  int  soinfo_size = sizeof(soinfo);
     si = (soinfo*) malloc(soinfo_size); //这里使用的是malloc，导致运行function的时候出现了错误
     if(si== NULL){
         DL_ERR("内存分配失败 malloc soinfo error !");
         return NULL;
     }
*********************************************
     更改了方法：
     /**
      * soinfo内存分配函数
      * */
     static soinfo* soinfo_alloc(){
         soinfo *si= reinterpret_cast<soinfo*>(mmap(NULL,PAGE_SIZE,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,0,0)); //获取新的内存页
         if(si == NULL){
             DL_ERR("内存分配失败");
             abort();
         }
         DL_INFO("内存分配成功 ！");
         memset(si,0,PAGE_SIZE);//内存初始化，
         DL_INFO("内存初始化完成");
         return si;
     }
     还是没有加载起来。


     ******************************
     2017-12-8
     根据排查的问题，发现DT_INIT函数没有被赋值，但是却被调用了。？？程序在运行的时候并没有执行到DT_INIT段。
     需要再看看程序.
     2017-12-13
     发现在LoadSegments调用mmap函数，出现couldn't mmap "/data/local/tmp/libMini_elf_loader.so" segment 10910 :Permission denied
     猜测是文件属性，因此重新调整文件。保存到了/data/data/com.loopher.loader/files/elf_loader.so
     /////////////////////////////////////////
     经过调试发现，确实是文件属性权限问题，调整为对应app目录下，就能正常load和link

     ****************************************
     2017-12-13
     测试使用的jni放在了doc/jni目录 ndk-build APP_ABI ="armeabi"
     在测试libfoo.so的时候，出现了如下的错误
     02-21 00:45:45.269 3802-3802/? I/Loopher: call INIT_ARRAY functions
     02-21 00:45:45.269 3802-3802/? I/Loopher: 调用 "function"构造函数 addr =0xc79
     在调用函数的构造函数的时候，出现了内存错误。明显函数地址不正确。
     -----------------------------
     出现调用 "function"构造函数 addr =0xc79 是因为在libfoo.so中存在
    void init() __attribute__((constructor));

    void init(){  //这个程序会最早被执行
        LOGD("Init test!");
    }
    在ida中查看这个so文件，发现在.init_array段中
    .text:00000C78 init   这里在被调用前，是地址+1，这里是thumb的模式。所以才会调用时才会输出 addr =0xc79
    也就是表明，程序读取到的还是物理内存上的地址，不是内存中的地址
     //////////////////////////
     在调用构造函数时，为什么会出现这个问题？？只调用到了物理地址，而不是内存中的地址
     ################################
     新的问题
     1、为什么在调用DT_INIT_ARRAY段的构造函数时，只掉用到了相对地址，不是内存中的绝对地址？？
     函数调用时：
        02-21 00:45:45.269 3802-3802/? I/Loopher: call INIT_ARRAY functions
          02-21 00:45:45.269 3802-3802/? I/Loopher: 调用 "function"构造函数 addr =0xc79
      使用ida查看到对应的init函数：
        .text:00000C78 init
     2、使用JNI_OnLoad的方式，加载libfoo.so文件。在返回到加载的JNI_OnLoad函数地址时，出现了内存错误。
     ######
     通过JNI_OnLoad的方式调用：
     02-21 03:43:13.179 5255-5255/? I/Loopher: 成功****************》》》 real_JNI_OnLoad addr = 0x7505acc1
     02-21 03:43:13.179 5255-5255/? E/Loopher:  find sym JNI_OnLoad  addr = 0x7505acc1
     02-21 03:43:13.179 5255-5255/? A/libc: Fatal signal 11 (SIGSEGV) at 0x00001eed (code=1), thread 5255 (.loopher.loader)
     ######
     错误信息：
     -21 03:43:13.339 174-174/? I/DEBUG:     d0  6d3fb1286d3fb0b0  d1  6d3fb1986d3fb1b2
     02-21 03:43:13.339 174-174/? I/DEBUG:     d2  6d3fb2086d3fb13f  d3  6d3fb2786d3fb26d
     02-21 03:43:13.339 174-174/? I/DEBUG:     d4  6966206f666e6920  d5  2a2064656873696e
     02-21 03:43:13.339 174-174/? I/DEBUG:     d6  2a2a2a2a2a2a2a2a  d7  2a2a2a2a2a2a2a2a
     02-21 03:43:13.339 174-174/? I/DEBUG:     d8  0000000000000000  d9  0000000000000000
     02-21 03:43:13.339 174-174/? I/DEBUG:     d10 0000000000000000  d11 0000000000000000
     02-21 03:43:13.339 174-174/? I/DEBUG:     d12 0000000000000000  d13 0000000000000000
     02-21 03:43:13.339 174-174/? I/DEBUG:     d14 0000000000000000  d15 0000000000000000
     02-21 03:43:13.339 174-174/? I/DEBUG:     d16 3fe8000000000000  d17 3fc999999999999a
     02-21 03:43:13.339 174-174/? I/DEBUG:     d18 0000000000000000  d19 0000000000000000
     02-21 03:43:13.339 174-174/? I/DEBUG:     d20 0000000000000000  d21 0002000200020002
     02-21 03:43:13.339 174-174/? I/DEBUG:     d22 0000000000000000  d23 0000000000000000
     02-21 03:43:13.339 174-174/? I/DEBUG:     d24 0000000000000000  d25 0002a7600002a760
     02-21 03:43:13.339 174-174/? I/DEBUG:     d26 0707070703030303  d27 0300000004000000
     02-21 03:43:13.339 174-174/? I/DEBUG:     d28 0800000009000000  d29 0001000000010000
     02-21 03:43:13.339 174-174/? I/DEBUG:     d30 010b400001088000  d31 01108000010e0000
     02-21 03:43:13.339 174-174/? I/DEBUG:     scr 60000010
     02-21 03:43:13.339 174-174/? I/DEBUG: backtrace:
     02-21 03:43:13.339 174-174/? I/DEBUG:     #00  pc 0004de62  /system/lib/libdvm.so
     02-21 03:43:13.339 174-174/? I/DEBUG:     #01  pc 0003a6f9  /system/lib/libdvm.so
     02-21 03:43:13.339 174-174/? I/DEBUG:     #02  pc 00000cfb  /data/data/com.loopher.loader/files/libfoo.so (JNI_OnLoad+58)
     02-21 03:43:13.339 174-174/? I/DEBUG:     #03  pc 000047a9  /data/app-lib/com.loopher.loader-1/libnative-lib.so (JNI_OnLoad+180)
     02-21 03:43:13.339 174-174/? I/DEBUG:     #04  pc 00050005  /system/lib/libdvm.so (dvmLoadNativeCode(char const*, Object*, char**)+468)
     02-21 03:43:13.339 174-174/? I/DEBUG:     #05  pc 000676e5  /system/lib/libdvm.so
     02-21 03:43:13.339 174-174/? I/DEBUG:     #06  pc 00026fe0  /system/lib/libdvm.so
     02-21 03:43:13.339 174-174/? I/DEBUG:     #07  pc 0002dfa0  /system/lib/libdvm.so (dvmMterpStd(Thread*)+76)
     02-21 03:43:13.339 174-174/? I/DEBUG:     #08  pc 0002b638  /system/lib/libdvm.so (dvmInterpret(Thread*, Method const*, JValue*)+184)
     02-21 03:43:13.339 174-174/? I/DEBUG:     #09  pc 0006057d  /system/lib/libdvm.so (dvmCallMethodV(Thread*, Method const*, Object*, bool, JValue*, std::__va_list)+336)
     02-21 03:43:13.339 174-174/? I/DEBUG:     #10  pc 000605a1  /system/lib/libdvm.so (dvmCallMethod(Thread*, Method const*, Object*, JValue*, ...)+20)
     02-21 03:43:13.339 174-174/? I/DEBUG:     #11  pc 0006bbdd  /system/lib/libdvm.so (dvmInitClass+1020)
     02-21 03:43:13.339 174-174/? I/DEBUG:     #12  pc 00067003  /system/lib/libdvm.so
     02-21 03:43:13.339 174-174/? I/DEBUG:     #13  pc 00026fe0  /system/lib/libdvm.so