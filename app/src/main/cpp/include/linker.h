#pragma once
#include "elf.h"
#include <cwchar>

// Returns the address of the page containing address 'x'.
#define PAGE_START(x)  ((x)& PAGE_MASK)

// Returns the offset of address 'x' in its page.
#define PAGE_OFFSET(x) ((x) & ~PAGE_MASK)

// Returns the address of the next page after address 'x', unless 'x' is
// itself at the start of a page.
#define PAGE_END(x)    PAGE_START((x)+(PAGE_SIZE - 1))

// Android uses RELA for aarch64 and x86_64. mips64 still uses REL.
#if defined(__aarch64__) || defined(__x86_64__)
#define USE_RELA 1
#endif


#define FLAG_LINKED     0x00000001
#define FLAG_EXE        0x00000004 // The main executable
#define FLAG_LINKER     0x00000010 // The linker itself
#define FLAG_NEW_SOINFO 0x40000000 // new soinfo format

#define SOINFO_NAME_LEN 128
typedef void(*linker_function_t)();

struct soinfo;
//struct SoInfoListAllocator {
//public:
//	LinkedList
//
//};
struct soinfo {
	char name[SOINFO_NAME_LEN];
	const ElfW(Addr) *phdr;
	size_t phnum;
	ElfW(Addr) entry;
	ElfW(Addr) base;

	ElfW(Dyn)* dynamic;

	soinfo* next;
	unsigned flags;//是否被Linked的标志
	const char* strtab;//字符串表
	ElfW(Sym)* symtab;//符号表
	size_t nbucket;//链表
	size_t nchain;//单链表
	unsigned * bucket;
	unsigned * chain;

#if defined(__mips__) || ! defined(__LP64__)
	//
	ElfW(Addr) ** plt_got;//给 mpis和Mips64预留的空间，
#endif

#if defined(__arm__)
  // ARM EABI section used for stack unwinding.
      unsigned* ARM_exidx;
     size_t ARM_exidx_count;
#elif defined(__mips__)
     unsigned mips_symtabno;
     unsigned mips_local_gotno;
     unsigned mips_gotsym;
#endif

 size_t ref_count;
 bool constructors_called;

 // When you read a virtual address from the ELF file, add this
  // value to get the corresponding address in the process' address space.
    ElfW(Addr) load_bias; //so文件加载到内存中的基地址，需要通过计算得到

};

