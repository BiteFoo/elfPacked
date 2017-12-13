#include <elf.h>
#include "mylinker.h"
#include "reader/ElfReader.h"  //读取并解析so文件

// Created by John.Lu on 2017/12/4.
//在这里连接和装在打开的文件
static unsigned bitmask[4096];
#define MARK(offset) \
    do { \
        bitmask[((offset) >> 12) >> 3] |= (1 << (((offset) >> 12) & 7)); \
    } while (0)
/**
 * soinfo内存分配函数
 * */
static soinfo* soinfo_alloc(){
    soinfo *si= reinterpret_cast<soinfo*>(mmap(NULL,PAGE_SIZE,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,0,0));
    if(si == NULL){
        DL_ERR("内存分配失败");
        abort();
    }
    DL_INFO("内存分配成功 ！");
    memset(si,0,PAGE_SIZE);//清空新申请的内存
    return si;

}
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
    si =  soinfo_alloc();// malloc(soinfo_size);
    if(strlen(name) >= SOINFO_NAME_LEN){
            //这里需要检测一下so文件的长度大小
        DL_ERR("\"%s\" name is too long " ,name);
        return  NULL;
    }
    memset(si,0,soinfo_size);//内存初始化
    strlcpy(si->name,name,sizeof(si->name));//使用strlcpy()可以保证不会出现溢出，strcpy()可能会溢出不够安全
    si->flags=FLAG_NEW_SOINFO;
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
static void phdr_table_get_dynamic_section(const ElfW(Phdr)* phdr_table,size_t phdr_count,
                                           ElfW(Addr) load_bias, ElfW(Dyn)** dynamic,
                                           size_t * dynamic_count, ElfW(Word)* dynamic_flags)
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
                //Elf32_Dyn的结构，它占8字节。而PT_DYNAMIC段就是存放着Elf32_Dyn数组，所以dynamic_count的值就是该段的memsz/8。
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
static int phdr_table_get_arm_exidx(const ElfW(Phdr)* phdr_table,size_t phdr_count,
                                    ElfW(Addr) load_bias,ElfW(Addr)** arm_exidx,
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
    uint32_t needed_count=0;
    for(ElfW(Dyn)* d = si->dynamic;d->d_tag != DT_NULL;++d){
       DL_INFO("d= %p,d[0](tag) =%p , d[1](val) = %p",d, reinterpret_cast<void*>(d->d_tag),
               reinterpret_cast<void*>(d->d_un.d_val));
        switch (d->d_tag){
            case DT_HASH: //哈希表
                //这里的计算方式 这个链表
                si->nbucket = reinterpret_cast<uint32_t*>(base + d->d_un.d_ptr)[0];
                si->nchain = reinterpret_cast<uint32_t*>(base + d->d_un.d_ptr)[1];
                si->bucket = reinterpret_cast<uint32_t*>(base + d->d_un.d_ptr + 8);
                si->chain = reinterpret_cast<uint32_t*>(base + d->d_un.d_ptr + 8 + si->nbucket * 4);
                break;
            case DT_STRTAB://字符串表
                si->strtab = reinterpret_cast<const char*>(base + d->d_un.d_ptr);
//                si->strtab = reinterpret_cast<const char *>(base+d->d_un.d_ptr);
                break;
            case DT_SYMTAB: // 函数符号表
                si->symtab = reinterpret_cast<ElfW(Sym)*>(base+d->d_un.d_ptr);
                break;
//#if ! definfe (__LP64__) //指定为32bit
            // 32 bit
            case DT_PLTREL: //从定位表
                if(d->d_un.d_val != DT_REL){
                    DL_ERR("不支持 DT_REL ");
                    return false;
                }
                break;
//#endif
            case DT_JMPREL: //跳转表
                si->plt_rel = reinterpret_cast<ElfW(Rel)*> (base+d->d_un.d_ptr);
                break;
            case DT_PLTRELSZ://重定位表大小
                si->plt_rel_count = d->d_un.d_val /sizeof(ElfW(Rel));
                break;
            case DT_PLTGOT://全局偏移表
                //指定给mips 和mips64使用，暂时不管
                DL_ERR("不用管 DT_PLTGOT");
                break;
            case DT_DEBUG://用于调试，可以忽略
                //调试信息，暂时不管
                DL_ERR("调试字段 DT_DEBUG");
                break ;
            //32 bit
            case  DT_RELA:
                DL_ERR("不支持 DT_RELA ");
                return false;
            case DT_RELASZ:
                //指的是64bit，可以不用管
                break;
            case DT_REL:
                si->rel = reinterpret_cast<ElfW(Rel)*>(base+d->d_un.d_ptr);
                break;
            case DT_RELSZ:
                si->ref_count = d->d_un.d_val / sizeof(ElfW(Rel));
                break;
            case DT_INIT:
                //init函数 这里是给可执行文件使用到，.so文件不会调用
                DL_ERR("INIT函数");
                si->init_func = reinterpret_cast<linker_function_t >(base+d->d_un.d_ptr);
                break;
            case DT_FINI:
                //init 的析构函数
                DL_ERR("init 的析构函数");
                si->fini_func = reinterpret_cast<linker_function_t > (base+d->d_un.d_ptr);
                break;
            case DT_INIT_ARRAY:
                // 这里读取到的 d->d_un.d_ptr 刚好符合使用readelf -d libfoo.so的值
                //0x00000019 (INIT_ARRAY)                 0x3eb0
                DL_ERR("DT_INIT_ARRAY 的析构函数 d->d_un.d_ptr=%p",d->d_un.d_ptr); //.so文件使用的构造函数
                // DT_INIT_ARRAY 的析构函数 d->d_un.d_ptr=0x3eb0
                si->init_array = reinterpret_cast<linker_function_t *> (base+d->d_un.d_ptr);
                //
                break;
            case DT_INIT_ARRAYSZ:
                DL_ERR("DT_INIT_ARRAYSZ 的数量");
                si->init_array_count=((unsigned)d->d_un.d_val) /sizeof(ElfW(Addr));
                break;
            case DT_FINI_ARRAY://析构函数
                si->fini_array = reinterpret_cast<linker_function_t *> (base+d->d_un.d_ptr);
                break;
            case DT_FINI_ARRAYSZ:
                si->fini_array_count = ((unsigned)d->d_un.d_val) / sizeof(ElfW(Addr));
                break;
            case DT_PREINIT_ARRAY:
                si->preinit_array = reinterpret_cast<linker_function_t *> (base+d->d_un.d_ptr);//preinit_arrar函数
                break;
            case DT_PREINIT_ARRAYSZ:
                si->preinit_array_count = (((unsigned)d->d_un.d_val) / sizeof(ElfW(Addr)));
                break;
            case DT_TEXTREL:
                //没有处理64位，因为原始处理中64位是不做处理
                si->has_text_relocations=true;
                break;
            case DT_SYMBOLIC:
                si->has_DT_SYMBOLIC=true;
                break;
            case DT_NEEDED:
                ++needed_count;// 需要额外的链接库字段
                DL_ERR("DT_NEEDED...");
                break;
            case DT_FLAGS:
                if(d->d_un.d_val & DF_TEXTREL)
                {
                    si->has_text_relocations = true; // 32 bit use
                }
                if(d->d_un.d_val & DF_SYMBOLIC){
                    si->has_DT_SYMBOLIC =true;
                }
                break;
            //需要添加mips处理
                //结束添加mips处理
            default:
                //没有找到符合的tag
                DL_INFO("没有使用的字段 type = %p , arg=%p", reinterpret_cast<void*>(d->d_tag),
                        reinterpret_cast<void*>(d->d_un.d_ptr));
                break;
        }
    }
    DL_ERR("**************************Extracting dynamic info finished ************************");
    if (relocating_linker && needed_count != 0) {
        DL_ERR("linker cannot have DT_NEEDED dependencies on other libraries");
        return false;
    }
    if (si->nbucket == 0) {
        DL_ERR("empty/missing DT_HASH in \"%s\" (built with --hash-style=gnu?)", si->name);
        return false;
    }
    if (si->strtab == 0) {
        DL_ERR("empty/missing DT_STRTAB in \"%s\"", si->name);
        return false;
    }
    if (si->symtab == 0) {
        DL_ERR("empty/missing DT_SYMTAB in \"%s\"", si->name);
        return false;
    }

    for(ElfW(Dyn)* d = si->dynamic;d->d_tag!= DT_NULL;++d){
        if(d->d_tag == DT_NEEDED){
            const char* library_name = si->strtab+d->d_un.d_val;
            DL_ERR("需要额外的动态库： %s",library_name);
            soinfo* lsi =(soinfo*)dlopen(library_name,RTLD_NOW);
            if(lsi != NULL){
                DL_INFO("找到\"%s\" 库, addr = %p",library_name,lsi);
                // 这里准备一个链表，用来保存需要的动态库，
                //solist.add(sli);
            }
//             char* lib_dit = "/system/lib";
//            strcat(lib_dit,library_name);
//            DL_INFO("动态库路径： %s",lib_dit);
        }
    }
    //
    DL_INFO("处理重定向表修复...");
    if(si->plt_rel != NULL){
        DL_INFO("si->plt_rel != NULL ================= ");
        if(soinfo_arm_type_relocate(si,si->plt_rel,si->plt_rel_count)){ //0表示成功 其他表示失败
            DL_ERR("si->plt_rel != NULL ================= relocate failed ");
            return false;
        }
    }
    if(si->rel != NULL){
        DL_INFO("si->rel != NULL ================= ");
        if(soinfo_arm_type_relocate(si,si->rel,si->rel_count)){
            DL_ERR("si->rel != NULL ================= relocate failed ");
            return false;
        }
    }
    DL_INFO("~~~~链接so完成 !~~~~");
    //在这里调用初始化函数
//    si->flags |= FLAG_EXE;
    si->flags |= FLAG_LINKED;
    if(si->init_func == NULL){
        DL_ERR("init_func函数没有初始化");
    }
    si->call_constructors();
    return true;
}

/**
 * 根据hash值，直接查找更快速
 * */
static unsigned  elfhash(const char* _name){
    const unsigned  char* name = reinterpret_cast<const unsigned  char*> (_name);
    unsigned  h,g;
    while (*name){
        h = (h<<4) + *name++;
        g = h & 0xf0000000;
        h ^= g;
        h ^= g >>24;
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
    DL_INFO("findSym -----------------------");
    hashval = elfhash(name);
    for(i = bucket[hashval % nbucket]; i != 0; i = chain[i]){
        if(symtab[i].st_shndx != 0){
            DL_INFO("符号名字： %s",strtab + symtab[i].st_name);
            if(strcmp(strtab + symtab[i].st_name, name) == 0){
                DL_INFO("找到符号地址： %p",symtab[i].st_value);
                return symtab[i].st_value;
            }
        }
    }
    return 0;
}


/**
 * arm处理器重定向表修复函数
 * */
static int soinfo_arm_type_relocate(soinfo* si,ElfW(Rel)* rel, unsigned count){
    ElfW(Sym)* s;
    int mmmmm =0;
    for(size_t idx =0;idx<count;++idx,++rel){
        unsigned  type = ELF32_R_TYPE(rel->r_info);
        unsigned  sym = ELF32_R_TYPE(rel->r_info);
        ElfW(Addr) reloc = static_cast<ElfW(Addr)>(rel->r_offset+si->load_bias);
        ElfW(Addr) sym_addr =0;
        const char* sym_name =NULL;
        if(type ==0){
            continue;
        }
//        DL_INFO("type=%d",type);
//        if(sym != 0)
//        {
//            //符号查找这里暂时不管
//            sym_name = reinterpret_cast<const char*>(si->strtab+si->symtab[sym].st_name);
////            s = soinfo_do_look_up(si,sym_name,NULL);
//            DL_ERR("********* sym_name :%s  ,mmmm =%d , rel=%p",sym_name,mmmmm,rel);
//            s= (Elf32_Sym *) findSym(si, sym_name);
//            mmmmm++;
//        }
//        mmmmm++;
        switch (type){
            case R_ARM_JUMP_SLOT:
                DL_INFO("-----------------R_ARM_JUMP_SLOT----------------");
//                DL_INFO("R_ARM_JUMP_SLOT ; count= %d",mmmmm);
                MARK(rel->r_offset);
                *reinterpret_cast<ElfW(Addr)*>(reloc) = sym_addr; //这里做修正地址
                break;
            case R_ARM_GLOB_DAT:
                DL_INFO("-----------------R_ARM_GLOB_DAT----------------");
                MARK(rel->r_offset);
                *reinterpret_cast<ElfW(Addr)*>(reloc) = sym_addr;
                break;
            case R_ARM_ABS32:
                DL_INFO("-----------------R_ARM_ABS32----------------");
                MARK(rel->r_offset);
                *reinterpret_cast<ElfW(Addr)*> (reloc) += sym_addr;
                break;
            case R_ARM_REL32:
                DL_INFO("-----------------R_ARM_REL32----------------");
                MARK(rel->r_offset);
                *reinterpret_cast<ElfW(Addr)*> (reloc) += sym_addr - rel->r_offset;
                break;
            case R_ARM_COPY:
                DL_INFO("-----------------R_ARM_COPY----------------");
                break;
            case R_ARM_RELATIVE:
                DL_INFO("-----------------R_ARM_RELATIVE----------------");
                *reinterpret_cast<ElfW(Addr)*>(reloc) += si->base;
                break;
            default:
                return -1;
        }
    }
    return 0;
}


void soinfo:: CallConstructor(const char* name,linker_function_t function){
    if (function == NULL || reinterpret_cast<uintptr_t>(function) == static_cast<uintptr_t>(-1)){
        DL_ERR("Constructor is NULL");
        return ;
    }
    DL_INFO("调用 \"%s\"构造函数 addr =%p ",name,function);
    function();
    DL_INFO("调用 \"%s\"构造函数  完成",name);
}
void soinfo :: CallArray(const char* array_name,linker_function_t* functions,size_t count ,bool reverse){
    if(functions == NULL){
            return;
        }
    int begin = reverse ? (count -1 ):0;
    int end  = reverse ? -1 :count;
    int step =reverse ? -1:1;
    for(int i=begin;i!=end;i+=step){
        CallConstructor("function",functions[i]);
    }
}

void soinfo::call_constructors(){
    DL_INFO("==================call_constructors===================");
    if(preinit_array){
        DL_ERR("WARNING: \"%s\" DT_PREINIT_ARRAY is INVALID ",name);
    }
    if(flags & FLAG_EXE ==0){
    DL_ERR("WARNING: flags & FLAG_EXE ==0");
    }
    //还需要添加额外的动态库的链接
    if(init_func)
    {
        //可执行文件
        CallConstructor("DT_INIT",init_func);//怎么会出现没有init函数？？？
    }
    if(init_array)
    {
        //.so文件
        DL_INFO("call INIT_ARRAY functions ");
        CallArray("DT_INIT_ARRAY",init_array,init_array_count, false);
    }

}


