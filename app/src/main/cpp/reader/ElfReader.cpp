#include "ElfReader.h"
ElfReader::~ElfReader() {
    if(phdr_mmap_ !=NULL){
        munmap(phdr_mmap_,phdr_size_); //释放内存
    }

}

ElfReader::ElfReader(const char *soname, int fd):name_(soname),fd_(fd),phdr_num_(0),
phdr_mmap_(NULL),phdr_table_(NULL),phdr_size_(0),load_start_(NULL),load_size_(0),load_bias_(NULL),
                                                 loaded_phdr_(NULL)
{
    //初始化
}
//
//size_t ElfReader::phdr_count() {
//    return phdr_num_;
//}
/**
 *
 * 读取elf header 信息
 * */
bool ElfReader::ReadElfHeader() {
    ssize_t  rc = TEMP_FAILURE_RETRY(read(fd_,&header_,sizeof(header_)));
    if(rc<0){
        DL_ERR("can't read file \"%s\"",name_);
        return false;
    }
    if (rc != sizeof(header_)){
        DL_ERR(" file \"%s\" too small to be an ELF ,only found %zd bytes",name_,rc);
        return false;
    }
    return true;
}
/**
 * 校验 elf 的
 * magic
 * class
 * endia-tag
 * e_type: ET_DYN
 * e_machine :x86 or arm   EM_ARM EM_
 * e_version
 * */
bool ElfReader::VerifyElfHeader() {
    DL_INFO("---------------VerifyElfHeader-------------------");
    if(header_.e_ident[EI_MAG0] != 0x7f || header_.e_ident[EI_MAG1] != 0X45 ||
            header_.e_ident[EI_MAG2] != 0x4c || header_.e_ident[EI_MAG3] != 0x46){
        DL_ERR(" ELF magic is error :\"%s\"",name_);
        return false;
    }
//    if(memcmp(header_.e_ident,ELFMAG,SELFMAG) !=0){//这里判断0x7fELF 4个字节长度
//        DL_ERR(" ELF magic is error :\"%s\"",name_);
//
//        DL_ERR(" ELF magic is magic0=0x%x , magic1 =0x%x,magic2=0x%x,magic3=0x%x :",magic0,magic1,magic2,magic3);
//        return false;
//    }
    int elf_class = header_.e_ident[EI_CLASS];
    if(elf_class ==ELFCLASS32){
        //32bit
        DL_INFO("ELF is 32bit class");
    }
    if(elf_class == ELFCLASS64){
        //64bit
        DL_INFO("ELF is 64bit class");
    }
    if(header_.e_ident[EI_DATA] !=ELFDATA2LSB){//不是小端存储类型
        DL_ERR("\"%s\" not little-endian: %d", name_, header_.e_ident[EI_DATA]);
        return false;
    }
    if(header_.e_type != ET_DYN){
        DL_ERR("\"%s\" has unexpected e_type: %d", name_, header_.e_type);
        return false;
    }

    if(header_.e_version != EV_CURRENT){
        DL_ERR("\"%s\" has unexpected e_version: %d", name_, header_.e_version);
        return false;
    }
    if(header_.e_machine != EM_ARM ){//这里选择arm作为测试
            // 这里判断如果不是 arm 或者是 x86的芯片，则不能加载so
        DL_ERR("\"%s\" has unexpected  machine :%d",name_,header_.e_machine);
        return false;
    }
    //暂时不管x86平台你
//    if ( header_.e_machine != EM_386){
//
//    }
    return true;
}

bool ElfReader::ReadProgramHeader() {
    DL_INFO("-------------ReadProgramHeader----------------------");
    phdr_num_ = header_.e_phnum;//读取program header table 的number值
    DL_INFO("phdr_num_ =%d",phdr_num_);
    DL_INFO("e_phoff=0x%x",header_.e_phoff);
    // program header table must smaller than 64KiB
    if(phdr_num_ <1 || phdr_num_ >65536 /sizeof(ElfW(Phdr))){
        DL_ERR("\"%s\" has invalid e_phnum: %zd", name_, phdr_num_);
        return false;
    }
    ElfW(Addr)  page_min = PAGE_START(header_.e_phoff);//开始映射起始地址
//    DL_INFO("page_min =0x%x",page_min);
    ElfW(Addr) page_end = PAGE_END(header_.e_phoff+(phdr_num_ * sizeof(ElfW(Phdr)))); //总的页边界结束地址
//    DL_INFO("page_end =0x%x",page_end);
    ElfW(Addr) page_offset = PAGE_OFFSET(header_.e_phoff);
//    DL_INFO("page_offset =0x%x",page_offset);
    phdr_size_  = page_end -page_min;//
//    DL_INFO("phdr_size_ =0x%x",phdr_size_);
    //做一个匿名映射到内存中
    void* mmap_addr = mmap(NULL,phdr_size_,PROT_READ,MAP_PRIVATE,fd_,page_min);
    if(mmap_addr == NULL){
        DL_ERR("  mmap \"%s\"  file  was failed ",name_);
        return false;
    }
    phdr_mmap_ = mmap_addr;//program header table 映射起始地址
    DL_INFO("phdr_mmap: 0x%x",(unsigned int)phdr_mmap_);
    //program header table min_addr  程序头部表的首地址 ，这里可以理解是一个数组表
    phdr_table_ = reinterpret_cast<ElfW(Phdr)* >(reinterpret_cast<char *> (mmap_addr) +page_offset);//为什么不是其他类型的指针，而是char*
    DL_INFO("phdr_table_: 0x%x",(unsigned int)phdr_table_);
    return true;
}

/**
 * 获取可加载段表的大小load_size_和可加载段表的首地址 load_start_
 * */
size_t phdr_get_load_size(const ElfW(Phdr) * phdr_table,size_t phdr_count,ElfW(Addr)* out_min_addr,ElfW(Addr)* out_max_addr){
    ElfW(Addr)  min_addr =UINTPTR_MAX;
    ElfW(Addr)  max_addr =0 ;
    bool fount_pt_load= false;
    for(size_t i =0;i<phdr_count;++i){
        const ElfW(Phdr)* phdr = &phdr_table[i];
        if(phdr->p_type != PT_LOAD){
//            DL_ERR("not found PT_LOAD ");
            continue; //不是PT_LOAD，跳过当前
        }
//        DL_INFO("found ======= %d",i);
//        DL_INFO("phdr->p_vaddr = 0x%x",(unsigned int )phdr->p_vaddr);
        fount_pt_load =true;
        if(phdr->p_vaddr <min_addr){
            min_addr =phdr->p_vaddr;
        }
        if(phdr->p_vaddr+phdr->p_memsz > max_addr){
            max_addr = phdr->p_vaddr+phdr->p_memsz;
        }
    }
    if(!fount_pt_load){
        min_addr =0;
    }
    min_addr  = PAGE_START(min_addr);//做转换
    max_addr = PAGE_END(max_addr);
    if(out_max_addr !=NULL){
        *out_max_addr = max_addr;
    }
    if(out_min_addr!= NULL){
        *out_min_addr = min_addr;
    }
    return max_addr-min_addr; //可加载段表的大小 = 可加载段表的结束地址 - 可加载段表的起始地址
}

/*
 *
 * 这里计算出 加载器加载时的地址 load_bias_
 * 计算出so文件ke可加载段的大小值 load_size_
 * 计算出每个段加载时的起始地址 load_start_
 * **/
bool ElfReader::ReserveAddressSpec() {
    DL_INFO("---------------------ReserveAddressSpec----------------------");
    //首先，计算出可加载段的大小，每个可加载段的首地址
    ElfW(Addr) min_addr;
    load_size_ = phdr_get_load_size(phdr_table_,phdr_num_,&min_addr,NULL);//将每个段的地址返回
    if(load_size_ == 0){
            DL_ERR("not found PT_LOAD,and \"%s\" has no loadable sgments ",name_);
        return false;
    }
    DL_INFO("可加载表长度为 load_size_： %zd",load_size_);
    DL_INFO("可加载段的偏移地址为 min_addr：0x%x",(unsigned int ) min_addr);
    uint8_t * addr = reinterpret_cast<uint8_t *>( min_addr);
    void* start;
//    size_t reverse_size =0;
//    bool reversed_hint = true;
    int mmap_flags = MAP_PRIVATE | MAP_ANONYMOUS;//私有匿名映射标志，
    start = mmap(addr,load_size_,PROT_NONE,mmap_flags,-1,0);
    if (start == NULL){
        DL_ERR("couldn't reserve %zd bytes of address space for \"%s\"",load_size_,name_);
        return false;
    }
    DL_INFO("找到加载器加载地址 ,映射首个加载段的地址 start： 0x%x",(unsigned int) start);
    load_start_ = start;
    DL_INFO("找到可加载段表的首地址 load_start_： 0x%x",(unsigned int) load_start_);
    load_bias_ = reinterpret_cast<uint8_t *> (start) - addr;
    DL_INFO("找到加载器加载地址偏移常量值 load_bias_： 0x%x",(unsigned int) load_bias_);
    return true;
}

/*
 *
 * 加载段
 * 在计算到加载器的加载地址和可加载段表的大小、地址后，开始加载所有可加载的段
 *
 * **/
bool ElfReader::LoadSegments() {
    DL_INFO("---------------------LoadSegments---------------------");
    for(size_t i =0;i<phdr_num_;++i){
        const ElfW(Phdr)* phdr = &phdr_table_[i];
        if(phdr->p_type != PT_LOAD){

            continue;
        }
//        DL_ERR(" fount %d",i);
        // segments addresses in memory  计算段加载到内存中的地址
        ElfW(Addr) seg_start = phdr->p_vaddr+load_bias_;//得到段加载首地址
//        DL_INFO("seg_start =0x%x",(unsigned int)seg_start );

        ElfW(Addr) seg_end = seg_start+phdr->p_memsz;//得到每个段映射地址
//        DL_INFO("seg_end =0x%x",(unsigned int)seg_end );

        ElfW(Addr) seg_page_start = PAGE_START(seg_start);//计算出需要映射的页起始地址
//        DL_INFO("seg_page_start =0x%x",(unsigned int)seg_page_start );

        ElfW(Addr) seg_page_end  = PAGE_END(seg_end);//计算出需要映射的页的结束地址
//        DL_INFO("seg_page_end =0x%x",(unsigned int)seg_page_end );
        ElfW(Addr) seg_file_end = seg_start+phdr->p_filesz;//计算出段的文件大小地址在映射页中的位置

//        DL_INFO("seg_file_end =0x%x",(unsigned int)seg_file_end );
        //计算出文件偏移
        ElfW(Addr) file_start = phdr->p_offset;//计算出文件占据内存的起始地址
//        DL_INFO("file_start =0x%x",(unsigned int)file_start );
        ElfW(Addr) file_end = file_start+phdr->p_filesz;///计算文件占据的内存结束地址
//        DL_INFO("file_end =0x%x",(unsigned int)file_end );

        ElfW(Addr) file_page_start = PAGE_START(file_start);
//        DL_INFO("file_page_start =0x%x",(unsigned int)file_page_start );
        ElfW(Addr) file_length = file_end - file_page_start;
//        DL_INFO("file_length =0x%x",(unsigned int)file_length );
        //具体计算图，可以看img的装载so的内存分布计算图，这图很简单，
        //开始映射每segments段到内存中 ,这里，不再是单个段 而是所有相同段组成的segments段
        if(file_length != 0){
            void* seg_addr = mmap(reinterpret_cast<void*>(seg_page_start),
                                  file_length,
                                  PFLAGS_TO_PROT(phdr->p_flags),
                                  MAP_FIXED|MAP_PRIVATE,
                                  fd_,
                                  file_page_start);

            if(seg_addr == MAP_FAILED){
                DL_ERR("couldn't mmap \"%s\" segment %zd ",name_,file_length);
                return false;
            }
        }
        //if(phdr->p_flags &PF_W != 0 && PAGE_OFFSET(seg_file_end) >0){  //这里没有把phdr->p_flags &PF_W != 0
        //使用括号括起来，导致运算符的优先级别不同，出现了错误
        //如果segments的flags是可写状态，并且没有填满内存页的边界，那么需要将多余的部分使用0初始化
        //if(phdr->p_flags &PF_W != 0 && PAGE_OFFSET(seg_file_end) >0)
        if((phdr->p_flags &PF_W) != 0 && PAGE_OFFSET(seg_file_end) >0){ //这里是 seg_file_end        PAGE_SIZE
            memset(reinterpret_cast<void* >(seg_file_end),0,PAGE_SIZE - PAGE_OFFSET(seg_file_end));
        }
        seg_file_end = PAGE_END(seg_file_end);
        //可能seg_page_end >seg_file_end    ,需要将file_page_end 和seg_page_end之间的空隙用0填充，
        if(seg_page_end > seg_file_end){
//            DL_INFO("seg_page_end > seg_file_end");
            void* zero_mmap= mmap(reinterpret_cast<void*> (seg_file_end),
                                  seg_page_end-seg_file_end,
                                  PFLAGS_TO_PROT(phdr->p_flags),
                             MAP_FIXED |MAP_ANONYMOUS|MAP_PRIVATE,
                                  -1,
                                  0
                               );
            if(zero_mmap == MAP_FAILED){
                DL_ERR("couldn't zero fill \"%s\",gap:%s",name_,strerror(errno));
                return false;
            }
        }
    }
    DL_INFO("LoadSegments ok  !!");
    return true;
}
/**
 *
 * 根据loadSegments完成后，会得到一个 PT_PHDR 的类型标志，这个类型只有可加载的segments被装载后，才会存在，并且只会存在0或者1次
 *这里需要找到这个表的首地址
 *同时检测这个PT_PHDR 表的合法性，为后续的linke_img阶段做预处理，检测
 * **/
bool ElfReader::FindPhdr() {
    DL_INFO("---------------FindPhdr----------------------");
    //两种情况
    //1、可能一开始就存在PT_PHDR这个表的项，那么这里直接返回
    //2、第二种，如果PT_PHDR不存在，那么需要从第一个加载的segment开始，找到第一个offset==0的位置，开始查找
    //根据第一个offset ==0 的文件开始加载program header
    const ElfW(Phdr)* phdr_limit = phdr_table_ + phdr_num_;//所有可加载段的总大小
    //第一种情况   // If there is a PT_PHDR, use it directly.
    for(const ElfW(Phdr) * phdr = phdr_table_;phdr <phdr_limit;++phdr){
        if(phdr->p_type == PT_PHDR){//如果找到，那么就直接返回
            DL_INFO("查找PT_PHDR段表，第一种情况 ***********");
            return CheckPhdr(load_bias_+phdr->p_vaddr);
        }
    }
    DL_INFO("查找PT_PHDR段表，第二种情况*************************》》》》》");
    //第二种情况
    // Otherwise, check the first loadable segment. If its file offset
    // is 0, it starts with the ELF header, and we can trivially find the
    // loaded program header from it.
    for(const ElfW(Phdr) * phdr; phdr<phdr_limit;++phdr){
        if(phdr->p_type == PT_LOAD){
            if(phdr->p_offset == 0){
                const ElfW(Addr) elf_addr = load_bias_+phdr->p_vaddr;
                const ElfW(Ehdr)* ehdr = reinterpret_cast<ElfW(Ehdr*> (elf_addr));
                ElfW(Addr) offset = ehdr->e_phoff;
                return CheckPhdr((ElfW(Addr))ehdr+offset);
            }
            break;//找到
        }
    }
    return false;
}
//可能不存在，这里需要确保program header 确实被加载到了内存中的segments中，这样做可以避免在后续的linke阶段出现异常
bool ElfReader::CheckPhdr(Elf32_Addr loaded) {
    const ElfW(Phdr)* phdr_limit = phdr_table_+phdr_num_;
    ElfW(Addr) loaded_end = loaded+(phdr_num_ * sizeof(ElfW(Addr)));
    for(ElfW(Phdr) *phdr = phdr_table_;phdr<phdr_limit;++phdr){
        if(phdr->p_type != PT_LOAD){
            continue;
        }
        ElfW(Addr) seg_start = phdr->p_vaddr + load_bias_;
        ElfW(Addr) seg_end = phdr->p_memsz+ seg_start;
        if(seg_start<= loaded && loaded_end <=seg_end){
            loaded_phdr_ = reinterpret_cast<const ElfW(Phdr) * >(loaded);
            DL_INFO("找到 loaded_phdr_ = 0x%x",(unsigned int) loaded_phdr_);
            return true;
        }
    }
    return false;
}

bool ElfReader::Load(void *) {
    return ReadElfHeader() &&
            VerifyElfHeader() &&
            ReadProgramHeader() &&
            ReserveAddressSpec() &&
            LoadSegments() &&
            FindPhdr();
}
