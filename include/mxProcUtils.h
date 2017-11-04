/*
*  Copyright Murex S.A.S., 2003-2013. All Rights Reserved.
*
*  This software program is proprietary and confidential to Murex S.A.S and its affiliates ("Murex") and, without limiting the generality of the foregoing reservation of rights, shall not be accessed, used, reproduced or distributed without the
*  express prior written consent of Murex and subject to the applicable Murex licensing terms. Any modification or removal of this copyright notice is expressly prohibited.
*/
#ifndef MXPROCUTILS_H
#define MXPROCUTILS_H

#include <elf.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined (_LP64)
#define Elf_Sym Elf64_Sym
#define Elf_Phdr Elf64_Phdr
#define Elf_Shdr Elf64_Shdr
#define Elf_Nhdr Elf64_Nhdr
#define Elf_Ehdr Elf64_Ehdr
#define Elf_Dyn  Elf64_Dyn
#define Elf_Addr Elf64_Addr
#define Elf_Off  Elf64_Off
// 64bit implimentations only use 48bit addressing at this stage
#define FMT_ADR "0x%012lx"
#else
#define Elf_Sym Elf32_Sym
#define Elf_Phdr Elf32_Phdr
#define Elf_Shdr Elf32_Shdr
#define Elf_Nhdr Elf32_Nhdr
#define Elf_Ehdr Elf32_Ehdr
#define Elf_Dyn  Elf32_Dyn
#define Elf_Addr Elf32_Addr
#define Elf_Off  Elf32_Off
#define FMT_ADR "0x%08lx"
#endif

#ifdef __sun
typedef struct stat64 mxstat;
#else
typedef struct stat mxstat;
#endif

typedef enum
{
   mxProcTypeNone = 0,
   mxProcTypeCore,
   mxProcTypePID
}
mxProcType;

typedef struct
{
   int lwpID;
   Elf_Addr fp;
   Elf_Addr sp;
   Elf_Addr ip;
   Elf_Addr stack;
   size_t stacksize;
}
mxLWP_t;

typedef struct
{
   int nph;
   const Elf_Phdr *ph;
   Elf_Addr baseAddr;
}
mxPHeaders_t;

typedef struct
{
   const char *strings;
   const Elf_Sym *table;
   int size;
   Elf_Addr baseAddr;
}
mxSymTab_t;

union mxArgValue
{
   unsigned long val; // Default
   unsigned int val4;
   unsigned char val1;
   double valDouble;
   float valFloat;
   char valChar[8];
};

typedef struct
{
   union mxArgValue val;
   int size; // 1,4 or 8 bytes are supported
   char *type;
}
mxArgument;

#define MAX_ARGS 100
typedef struct
{
   // Arguments from instrumentation
   mxArgument instArg[MAX_ARGS];
   int instCount;
   // Arguments passed on the stack
   mxArgument stackArg[MAX_ARGS];
   int stackCount;
   // Arguments passed on integer registers.  Only used on x86 64bit.
   mxArgument intArg[MAX_ARGS];
   int intCount;
   // Arguments passed on float registers  Only used on x86 64bit.
   mxArgument floatArg[MAX_ARGS];
   int floatCount;

   // Merged list of arguments, including type information
   mxArgument arg[MAX_ARGS];
   int count;

}
mxArguments;

#define MAX_LWPS 1000
typedef mxLWP_t mxLWPs_t[MAX_LWPS];

#define MAX_SYMTABS 2000
typedef mxSymTab_t mxSymTabs_t[MAX_SYMTABS];

typedef struct
{
   int fd;                // File descriptor
   void *mmloc;           // mmap location - not necessarily full file
   size_t mmsize;         // Size of mmap
   mxPHeaders_t phs;      // Program Headers
   char *fileName;        // Filename
   mxstat stat;
}
mxElfFile;

#define MAX_ELF_FILES 512
typedef struct
{
   mxProcType type;

   // Symbol Tables
   mxSymTabs_t symtab;
   int nsymtabs;

   // For Elf files
   int elfOpen;
   mxElfFile elfFile[MAX_ELF_FILES];

   // For PID
   int as;                      // file descriptor pointing to the address space
   pid_t pid;                   // PID of process

   int nLWPs;
   mxLWPs_t LWPs;

   char filePrefix[1024];      // If we want to dump some output to files, we can use this for the prefix
   char binFile[1024];         // Detected binary name
}
mxProc;

// Public API
mxProc *openCoreFile(const char *binFileName, const char *coreFileName, const char *libraryRoot, int plddMode);
mxProc *openPID(const char *binFileName, const char *PID, int plddMode);
void closeMxProc(mxProc *p);
void checkConsistency(mxProc * p, int force);

void printCallStack(const mxProc *p, mxLWP_t t, int fullStack, int stackArguments, int corruptStackSearch);
void dumpStack(const mxProc *p, mxLWP_t t, int words);
Elf_Addr getSymbolAddress(const mxProc * c, const char *symbolName);
void printpmap(mxProc *c);

int read_int(const mxProc *p, Elf_Addr vmAddr);
long read_long(const mxProc *p, Elf_Addr vmAddr);
Elf_Addr read_addr(const mxProc *p, Elf_Addr vmAddr);
double read_double(const mxProc *p, Elf_Addr vmAddr);
void read_string(const mxProc *p, Elf_Addr vmAddr, char *buf, size_t buf_length);
void read_bytes(const mxProc *p, Elf_Addr vmAddr, void *buf, size_t buf_length);

// Debugging
void setVerbose(int v);
void debug(const char *format, ...);
void warning(const char *format, ...);
void fatal_error(const char *format, ...);

char *get_function_name_from_prototype(char *name);


// Functions requires for OS/Arch specific code
void initMxProc(mxProc *m);
void loadSymbols(mxProc * p, int elfID, Elf_Addr baseAddr);
void loadLibraries(mxProc * p, int elfID, const char *libraryFile, int plddMode);
void readFile(const mxProc * c, int elfID, Elf_Addr fileAddr, void *buffPointer, size_t size);

enum {COREFIRST, CORELAST, COREONLY, FILEONLY};
void getFileAddrFromCore(const mxProc *c, Elf_Addr vmAddr, Elf_Addr *fileAddr, int *elfFile, int so);

void printStackItem(const mxProc *p, Elf_Addr addr, Elf_Addr argsAddr, int fullStack, int stackArguments);
const char *getUnknownSymbol();
int openElfFile(mxProc *c,  const char *fileName, Elf_Addr baseAddr, int justHeaders, int failIfInvalid);
mxArguments *getArguments(const mxProc *proc, Elf_Addr disAddr, Elf_Addr frameAddr, int verbose);
void addInstrumentedArgument(const mxProc *proc, mxArguments *args, int argNumber, Elf_Addr argAddr, int argLength);
void addStackArgument(const mxProc *proc, mxArguments *args, int argNumber, Elf_Addr argAddr, int argLength);
void addIntArgument(const mxProc *proc, mxArguments *args, int argNumber, Elf_Addr argAddr, int argLength);
void addFloatArgument(const mxProc *proc, mxArguments *args, int argNumber, Elf_Addr argAddr, int argLength);

// OS Specific functions
void closeMxProcPID(mxProc *p);
void getLWPsFromPID(mxProc *p);
void getLWPsFromCore(mxProc *p);
int readMxProcVM(const mxProc *p, Elf_Addr vmAddr, void *buff, size_t size);
void demangleSymbolName(const char *symbolName, char *demangled, int size);
Elf_Addr processSignalHandler(const mxProc * p, Elf_Addr stackLimit, Elf_Addr fp, Elf_Addr curr_ret_addr, int fullStack);


// A special memory location reading from core files which indicates that it is a valid, but unused address
// In this case, all reads should return NULL values. This will never conflict with a genuine location
// as the Elf headers are at the start of the file
#define ADDR_NULLVALUES 0x1

#endif



