//
// Created by John.Lu on 2017/12/4.
//

#include "ElfReader.h"

ElfReader::~ElfReader() {

}

ElfReader::ElfReader(const char *name, int fd) {

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
