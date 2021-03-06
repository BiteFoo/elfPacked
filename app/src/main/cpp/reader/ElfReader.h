//
// Created by John.Lu on 2017/12/4.
//装载so文件类

#ifndef ELFPACKED_ELFREADER_H
#define ELFPACKED_ELFREADER_H



#include <cwchar>
#include "../include/elf.h"
#include "../mylinker.h"

class ElfReader {
public:
    ElfReader(const char* name,int fd);
    ~ElfReader();
    bool Load(void*);
    size_t phdr_count(){return phdr_num_;}
    ElfW(Addr) load_start(){return reinterpret_cast<ElfW(Addr)> (load_start_);}
    size_t load_size(){return load_size_;}
    ElfW(Addr) load_bias(){return load_bias_;}
    const ElfW(Phdr)* loaded_phdr(){return loaded_phdr_;}

private:
    bool ReadElfHeader();
    bool  VerifyElfHeader();
    bool ReadProgramHeader();
    bool  ReserveAddressSpec();
    bool LoadSegments();
    bool FindPhdr();
    bool CheckPhdr(ElfW(Addr));
    const char* name_;
    int fd_;
    ElfW(Ehdr) header_;
    size_t phdr_num_;
    void* phdr_mmap_;
    ElfW(Phdr)* phdr_table_;
    ElfW(Addr) phdr_size_;
    //第一个页预留的地址空间
    void* load_start_;
    //预留空间的大小
    size_t load_size_;
    //Load bias  //可以理解为so文件加载到内存的基地址  因为加载到内存绝对地址 = phdr_offset+load_bias
    ElfW(Addr) load_bias_;
    //装载so的程序表的地址
    const ElfW(Phdr)* loaded_phdr_;

};


#endif //ELFPACKED_ELFREADER_H
