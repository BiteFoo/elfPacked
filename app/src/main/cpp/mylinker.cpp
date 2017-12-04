//
// Created by John.Lu on 2017/12/4.
//在这里连接和装在打开的文件

#include "mylinker.h"
#include "reader/ElfReader.h"  //读取并解析so文件
/**
 * 装载so文件
 * */
soinfo * load_so(int  fd){
    soinfo *si =NULL;
    if(fd  ==-1){
        return  NULL;
    }

    return si;

}

/**
 * 链接elf
 * */
void linke_so_img(soinfo * si){

}




