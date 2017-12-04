//
// Created by John.Lu on 2017/12/4.
//


#ifndef ELFPACKED_MYLINKER_H
#define ELFPACKED_MYLINKER_H

#include "../include/linker.h"

soinfo * load_so(const char* name ,int fd);
void linke_so_img(soinfo * si);


#endif //ELFPACKED_MYLINKER_H
