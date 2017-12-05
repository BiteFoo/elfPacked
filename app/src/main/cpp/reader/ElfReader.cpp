//
// Created by John.Lu on 2017/12/4.
//

#include "ElfReader.h"
#include <fcntl.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <sys/mman.h>

ElfReader::~ElfReader() {
    if(phdr_mmap_ !=NULL){
        munmap(phdr_mmap_,phdr_size_); //释放内存
    }

}

ElfReader::ElfReader(const char *soname, int fd):name(name),fd_(fd),phdr_num_(0),
phdr_mmap_(NULL),phdr_table_(NULL),phdr_size_(0),load_start_(NULL),load_size_(0),load_bias_(NULL),
                                                 loaded_phdr_(NULL)
{
    //初始化
}

size_t ElfReader::phdr_count() {
    return 0;
}

bool ElfReader::ReadElfHeader() {
    return false;
}

bool ElfReader::VerifyElfHeader() {
    return false;
}

bool ElfReader::ReadProgramHeader() {
    return false;
}

bool ElfReader::ReserveAddressSpec() {
    return false;
}

bool ElfReader::LoadSegments() {
    return false;
}

bool ElfReader::FindPhdr() {
    return false;
}

bool ElfReader::CheckPhdr(Elf64_Addr) {
    return false;
}

bool ElfReader::Load(void *) {

    return false;
}
