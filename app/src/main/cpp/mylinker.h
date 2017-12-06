//
// Created by John.Lu on 2017/12/4.
//


#ifndef ELFPACKED_MYLINKER_H
#define ELFPACKED_MYLINKER_H
#include <android/log.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include "../include/linker.h"
#define  LOG_TAG "Loopher"
#define  DL_ERR(fmt,x...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,fmt,##x)
#define  DL_INFO(fmt,x...) __android_log_print(ANDROID_LOG_INFO,LOG_TAG,fmt,##x)


#define MAYBE_MAP_FLAG(x, from, to)  (((x) & (from)) ? (to) : 0)
#define PFLAGS_TO_PROT(x)            (MAYBE_MAP_FLAG((x), PF_X, PROT_EXEC) | \
                                      MAYBE_MAP_FLAG((x), PF_R, PROT_READ) | \
                                      MAYBE_MAP_FLAG((x), PF_W, PROT_WRITE))
// Android uses RELA for aarch64 and x86_64. mips64 still uses REL.
#if defined(__aarch64__) || defined(__x86_64__)    //这里针对64cpu做处理 由于我这里不针对64位的处理器，所以不用管64处理的方式
#define USE_RELA 1
#endif

 soinfo * load_so(const char* name ,int fd);
 bool  linke_so_img(soinfo * si);

static void phdr_table_get_dynamic_section(const ElfW(Phdr)* phdr_table,size_t phdr_count,
                                ElfW(Addr) load_bias,ElfW(Dyn)** dynamic,size_t * dynamic_count,
                                ElfW(Word)* dynamic_flags );
static int phdr_table_get_arm_exidx(const ElfW(Phdr)* phdr_table,
                                    size_t phdr_count,
                                    ElfW(Addr) load_bias,
                                    ElfW(Addr)** arm_exidx,
                                    unsigned* arm_exidx_count);


#endif //ELFPACKED_MYLINKER_H
