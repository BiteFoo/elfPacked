关于如何编写自定义的linker，查看相关文章和对应的源码
源码：Android5.0
http://androidxref.com/5.0.0_r2/xref/bionic/linker/linker.cpp#open_library_on_path
文章：
关于如何自定义linker并且相关细节
http://blog.csdn.net/hgl868/article/details/52759921
有人写过并且给了源码，没有账号
http://www.pd521.com/thread-455-1-1.html
来自看雪的文章Thomasking的文章。

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