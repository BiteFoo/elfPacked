//
// Created by John.Lu on 2017/12/4.
//在这里连接和装在打开的文件

#include "mylinker.h"
#include "reader/ElfReader.h"  //读取并解析so文件

/**
 * 装载so文件
 * */
 soinfo * load_so(const char* name,int  fd){
    soinfo *si =NULL;
    if(fd  ==-1){
        return  NULL;
    }
    ElfReader elf_reader(name,fd);//构造函数
    if(!elf_reader.Load(NULL)){
        return NULL; //装载失败
    }
    DL_INFO("已经完成装载，准备分配内存空间");
    //需要分配soinfo的内存空间 分配空间，和链接工作，

    int  soinfo_size = sizeof(soinfo);
    si = (soinfo*) malloc(soinfo_size);
    if(si== NULL){
        DL_ERR("内存分配失败 malloc soinfo error !");
        return NULL;
    }
    else{
        if(strlen(name) >= SOINFO_NAME_LEN){
            //这里需要检测一下so文件的长度大小
            DL_ERR("\"%s\" name is too long " ,name);
            return  NULL;
        }
        DL_INFO("=内存分配成功=");
        memset(si,0,soinfo_size);//内存初始化
        strlcpy(si->name,name,sizeof(si->name));//使用strlcpy()可以保证不会出现溢出，strcpy()可能会溢出不够安全
        si->flags=FLAG_NEW_SOINFO;
    }
    DL_INFO("内存初始化完成@——@");
    DL_INFO("soinfo_size =0x%x",soinfo_size);
    si->base = elf_reader.load_start();
    si->size = elf_reader.load_size();
    si->load_bias = elf_reader.load_bias();
    si->phnum = elf_reader.phdr_count();
    si->phdr = elf_reader.loaded_phdr();
    return si;
}

/**
 * 这里根据 p_type== PT_DYNAMIC来查找动态段数据
 * 找到第一个之后，就返回，如果一直找不到，那么就返回失败值
 * */
static void phdr_table_get_dynamic_section(const ElfW(Phdr)* phdr_table,
                                           size_t phdr_count,
                                           ElfW(Addr) load_bias,
                                           ElfW(Dyn)** dynamic,
                                           size_t * dynamic_count,
                                           ElfW(Word)* dynamic_flags)
{
            const ElfW(Phdr)* phdr = phdr_table;
            const ElfW(Phdr)* phdr_limit = phdr_table+phdr_count;
        for(phdr =phdr_table;phdr<phdr_limit;phdr++){
            if(phdr->p_type !=PT_DYNAMIC){
                //不是动态段，跳过
                continue;
            }
            //找到动态段
            //给soinfo的dynamic赋值，
            *dynamic = reinterpret_cast<ElfW(Dyn)*>(phdr->p_vaddr + load_bias);
            if(dynamic_count){//如果dynamic 的地址不为空
                //这里需要给dynamic赋值
                *dynamic_count = ((unsigned)(phdr->p_memsz) /8);//这里是8字节一个
            }
            if(dynamic_flags){
                *dynamic_flags = phdr->p_flags;
            }
            //在找到第一个段的dynamic后，就返回，后续根据第一个段开始读取
            DL_INFO("找到第一个PT_DYNAMIC，完成查找");
            return ;
        }
    //没有找到，返回空
    DL_ERR("没有找到PT_DYNAMIC段，失败");
    *dynamic =NULL;
    if(dynamic_count){
        *dynamic_count =0;
    }

}


#if defined(__arm__)
#  ifndef PT_ARM_EXIDX
#    define PT_ARM_EXIDX    0x70000001      /* .ARM.exidx segment */
#  endif
#endif
/**
 * 查找arm处理器的相关信息
 * */
static int phdr_table_get_arm_exidx(const ElfW(Phdr)* phdr_table,
                                    size_t phdr_count,
                                    ElfW(Addr) load_bias,
                                    ElfW(Addr)** arm_exidx,
                                    unsigned* arm_exidx_count){

    const ElfW(Phdr)* phdr = phdr_table;
    const ElfW(Phdr)* phdr_limit = phdr_table+phdr_count;
    for(phdr=phdr_table;phdr<phdr_limit;phdr++){
        if(phdr->p_type != PT_ARM_EXIDX){
            //不是arm_exidx，跳过
            continue;
        }
        //找到 arm_exidx 的段区
        *arm_exidx = reinterpret_cast<ElfW(Addr)*> (load_bias+phdr->p_vaddr);
        *arm_exidx_count =((unsigned) phdr->p_memsz /8);//也是8个字节
        DL_INFO("找到.ARM.exidx section 在内存中的位置 ,地址为：0x%x ,数量为：0x%x",(unsigned int )*arm_exidx,(unsigned int )*arm_exidx_count);
        return 0;
    }
    DL_ERR("没有找到.ARM.exidx.section的位置");
    *arm_exidx =NULL;
    *arm_exidx_count =NULL;
    return -1;
}


/**
 * 链接elf
 * */
 bool linke_so_img(soinfo * si){

    DL_INFO("====================linke_so_img==========================");
    DL_INFO("linke so ====>>> size = %d",si->size);
    ElfW(Addr) base = si->load_bias;//加载器的偏移地址，也是可加载段的基址地址
    const ElfW(Phdr)* phdr = si->phdr; //可加载段表的首地址 PT_PHDR表或者是loaded segment 的第一个地址
    int phnum = si->phnum;
    bool relocating_linker = (si->flags & FLAG_LINKER) != 0;//  如果不为0 ，那么是重定位链接
    DL_INFO("准备开始链接工作 :\"%s\" ",si->name);
    DL_INFO("Extracting  dynamic section .....");
    //
    size_t dynamic_count ;
    ElfW(Word) dynamic_flags;
    //分别读取 si->dynamic（给soinfo的动态段赋值） dynamic_count（动态段的大小） dynamic_flags （访问的标志）
    phdr_table_get_dynamic_section(phdr,phnum,base,&si->dynamic,&dynamic_count,&dynamic_flags);//这里根据p_type == PT_DYNAMIC 来读取
    if(si->dynamic == NULL){
        DL_ERR("没有找到dynamic段，似乎 \"%s\" 没有PT_DYNAMIC",si->name);
        return false;
    }
    DL_INFO("第一个动态段的地址：0x%x ,数量为：%d, 访问标志位：0x%x",(unsigned int )si->dynamic,dynamic_count,(unsigned int )dynamic_flags);

    //这里现针对arm来做测试
#if defined(__arm__)
    DL_INFO("《《《《《《《ARM处理器》》》》》》》");
    phdr_table_get_arm_exidx(phdr,phnum,base,&si->ARM_exidx,&si->ARM_exidx_count);
#endif


    DL_INFO("开始读取动态段信息.....");






    return true;

}




