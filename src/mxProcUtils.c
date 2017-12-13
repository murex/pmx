/*
*  Copyright Murex S.A.S., 2003-2013. All Rights Reserved.
*
*  This software program is proprietary and confidential to Murex S.A.S and its affiliates ("Murex") and, without limiting the generality of the foregoing reservation of rights, shall not be accessed, used, reproduced or distributed without the
*  express prior written consent of Murex and subject to the applicable Murex licensing terms. Any modification or removal of this copyright notice is expressly prohibited.
*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <link.h>
#include <libgen.h>

#include "mxProcUtils.h"
#include "pmx.h"
#include "pmxsupport.h"

//#include "lib/common/mxconst/h/mx_version.h"

static const char *unknownSymbol = "??????";

void initMxProc(mxProc * p)
{
   memset(p, 0, sizeof(mxProc));
   p->type = mxProcTypeNone;
}

static int verbose=0;

void setVerbose(int v)
{
   verbose=1;
}

void debug(const char *format, ...)
{
   va_list ap;

   va_start(ap, format);

   if (verbose)
   {
      fprintf(stdout, " [debug] ");
      vfprintf(stdout, format, ap);
      fprintf(stdout, "\n");
   }

   va_end(ap);
}

void warning(const char *format, ...)
{
   va_list ap;

   va_start(ap, format);
   fprintf(stdout, "Warning: ");
   vfprintf(stdout, format, ap);
   fprintf(stdout, "\n");
   va_end(ap);
}

void fatal_error(const char *format, ...)
{
   va_list ap;

   va_start(ap, format);
   fprintf(stderr, "Fatal Error: ");
   vfprintf(stderr, format, ap);
   fprintf(stderr, "\n");
   va_end(ap);

   exit(EXIT_FAILURE);
}

void *mmapFile(mxProc * p, const char *fileName, int *fd, size_t * size, mxstat *sb)
{
   void *addr;

   if (!*fd)
   {
#ifdef __sun
      *fd = open64(fileName, O_RDONLY);
#else
      *fd = open(fileName, O_RDONLY);
#endif

      if (*fd == -1)
      {
         perror("open: ");
         fatal_error("Error opening %s", fileName);
      }

      int fstatResult=0;
#ifdef __sun
      fstatResult = fstat64(*fd, sb);
#else
      fstatResult = fstat(*fd, sb);
#endif

      if (fstatResult == -1)
      {
         perror("fstat: ");
         fatal_error("Error statting  %s", fileName);
      }

      if (*size==0)
         *size = sb->st_size;
   }

   addr = mmap(0, *size, PROT_READ, MAP_SHARED, *fd, 0);
   if (addr == MAP_FAILED)
      fatal_error("MMAP failed on %s with errno %d", fileName, errno);

   return addr;
}

void dumpSymbolTable(mxProc * p, int i)
{
   const Elf_Sym *symbol = p->symtab[i].table;
   int items = p->symtab[i].size / sizeof(Elf_Sym);
   int j = 0;

   for (j = 0; j < items; j++, symbol++)
   {
      if ((p->symtab[i].strings + symbol->st_name)[0])
      {
         // Don't print nameless symbols
         printf("found symbol " FMT_ADR " (" FMT_ADR "+" FMT_ADR ") sized %#lx, named %s\n", (unsigned long) symbol->st_value + (unsigned long) p->symtab[i].baseAddr,
                (unsigned long) p->symtab[i].baseAddr, (unsigned long) symbol->st_value, (unsigned long) symbol->st_size,
                p->symtab[i].strings + symbol->st_name);
      }
   }
}

const char *machineTypeToString(Elf32_Half m)
{
   switch(m) {
      case EM_386 :
         return "32bit_x86";
      case EM_X86_64  :
         return "64bit_x86";
      case EM_SPARC :
         return "32bit_sparc";
      case EM_SPARCV9 :
         return "64bit_sparc";
      default :
         return "Unknown";
   }
}

const char *osTypeToString(char c)
{
   switch(c) {
      case ELFOSABI_LINUX :
         return "Linux";
      case ELFOSABI_SOLARIS :
         return "Solaris";
      default :
         return "Unknown";
   }
}

int isValidElf(mxProc *p, const Elf_Ehdr * elfHdr)
{
   if (elfHdr->e_ident[EI_MAG0] != ELFMAG0 ||
       elfHdr->e_ident[EI_MAG1] != ELFMAG1 || elfHdr->e_ident[EI_MAG2] != ELFMAG2 || elfHdr->e_ident[EI_MAG3] != ELFMAG3)
   {
      warning("ELF magic numers don't match. This doesn't appear to be an ELF file.");
      return 0;
   }

   if (elfHdr->e_version != EV_CURRENT)
   {
      warning("ELF version don't match. Possible platform mismatch (x86 vs sparc).");
      return 0;
   }

   Elf32_Half foundMachine = elfHdr->e_machine;

#if (defined(__sparc) && defined (_LP64))
   Elf32_Half expectedMachine = EM_SPARCV9;

#elif defined(__sparc)
   Elf32_Half expectedMachine = EM_SPARC;
   if (foundMachine == EM_SPARC32PLUS)
      foundMachine = EM_SPARC;

#elif defined (_LP64)
   Elf32_Half expectedMachine = EM_X86_64;

#else
   Elf32_Half expectedMachine = EM_386;

#endif

   if (foundMachine != expectedMachine)
   {
      warning("ELF machine type mismatch. This pmx version can handle %s (%d) but we found %s (%d)",
              machineTypeToString(expectedMachine), expectedMachine, machineTypeToString(foundMachine), foundMachine);
      return 0;
   }

#if defined(__linux)
   char expectedOS = ELFOSABI_LINUX;
#else
   char expectedOS = ELFOSABI_SOLARIS;
#endif

   char foundOS = elfHdr->e_ident[EI_OSABI];
   if (foundOS != ELFOSABI_NONE && foundOS != expectedOS)
   {
      warning("ELF OS/ABI mismatch.  Expected %s (%d), found %s (%d).", osTypeToString(expectedOS), expectedOS, osTypeToString(foundOS), foundOS);
      return 1;
   }

   // Check program header size fits our structure.  Not sure why this would ever be wrong.
   if (elfHdr->e_phnum && elfHdr->e_phentsize != sizeof(Elf_Phdr))
   {
      warning("ELF Program Header size incorrect.  Expected %ld, found %d.", sizeof(Elf_Phdr), elfHdr->e_phentsize);
      return 1;
   }

   // Check section header size fits our structure.  Not sure why this would ever be wrong.
   if (elfHdr->e_shnum && elfHdr->e_shentsize != sizeof(Elf_Shdr))
   {
      warning("ELF Section Header size incorrect.  Expected %ld, found %d.", sizeof(Elf_Shdr), elfHdr->e_shentsize);
      return 1;
   }

   return 1;
}

void loadSymbols(mxProc * p, int elfID, Elf_Addr baseAddr)
{
   // Make sure we don't over run the buffer
   if (p->nsymtabs >= MAX_SYMTABS)
      return;

   const char * mmFile = reinterpret_cast <const char *>(p->elfFile[elfID].mmloc);

   // Read ELF File header and confirm it's an elf file
   const Elf_Ehdr *elfHdr = reinterpret_cast < const Elf_Ehdr * >(mmFile);

   if (elfHdr->e_shoff + elfHdr->e_shnum * sizeof(Elf_Shdr) > p->elfFile[elfID].mmsize)
   {
      // Section headers go outside our mapped area.  Happens sometimes inside core files.  If so, just ignore.
      // printf("Not looking for symbols here as it goes outside the mapped area\n");
      return;
   }

   // Read section and record if it has symbols
   const Elf_Shdr *secHdrs = reinterpret_cast < const Elf_Shdr * >(mmFile + elfHdr->e_shoff);

   for (int i = 0; i < elfHdr->e_shnum; i++)
   {
      if ((secHdrs[i].sh_type == SHT_SYMTAB || secHdrs[i].sh_type == SHT_DYNSYM) && secHdrs[i].sh_size)
      {
         if (secHdrs[i].sh_offset + secHdrs[i].sh_size > p->elfFile[elfID].mmsize)
         {
            //printf("NOT Adding symbol table from section %d from elfID %d with %d symbols as it goes over our loaded region\n", i, elfID, secHdrs[i].sh_size / sizeof(Elf_Sym));
            return;
         }
         else
         {
            //printf("Adding symbol table %d from section %d from elfID %d with %d symbols baseAddr " FMT_ADR "\n", p->nsymtabs,  i, elfID, secHdrs[i].sh_size / sizeof(Elf_Sym), baseAddr);
            int stringSection = secHdrs[i].sh_link;

            p->symtab[p->nsymtabs].strings = mmFile + secHdrs[stringSection].sh_offset;
            p->symtab[p->nsymtabs].size = secHdrs[i].sh_size;
            p->symtab[p->nsymtabs].table = reinterpret_cast < const Elf_Sym *>(mmFile + secHdrs[i].sh_offset);

            p->symtab[p->nsymtabs].baseAddr = baseAddr;
            //dumpSymbolTable(p,p->nsymtabs);
            p->nsymtabs++;
            if (p->nsymtabs >= MAX_SYMTABS)
               break;
         }
      }
      else {
         if (strcmp(".gnu_debuglink",reinterpret_cast <const char *>(mmFile + secHdrs[elfHdr->e_shstrndx].sh_offset+secHdrs[i].sh_name))==0)
         {
            const char *debugFile = reinterpret_cast < const char *>(mmFile + secHdrs[i].sh_offset);
            debug("Found .gnu_debuglink section [%d] with file [%s]",i, debugFile);

            char *debugBase=strdup(p->elfFile[elfID].fileName); // dirname can modify this, so we need a copy
            char *debugFilePath = (char *)malloc(strlen(debugFile) + strlen(debugBase) + 2);
            sprintf(debugFilePath,"%s/%s",dirname(debugBase),debugFile);
            free(debugBase);

            if (access(debugFilePath, R_OK) != -1)
            {
               debug("Loading Symbols from Debug File [%s]", debugFilePath);
               int elfID = openElfFile(p,debugFilePath,baseAddr,0,0);
               if (elfID)
               {
                  // Sometimes debug files have the PT_LOAD segments mirroring the original binary
                  // They are useless and get in the way, so lets pretend they don't exist
                  p->elfFile[elfID].phs.nph = 0;
                  loadSymbols(p, elfID, baseAddr);
               }
               else
               {
                  warning("Unable to open Debug File [%s].  Symbols will be unavailable.",debugFilePath);
               }
            }
            else {
               debug("Unable to open Debug File [%s].  Symbols will be unavailable.",debugFilePath);
            }
            free(debugFilePath);
         }
      }
   }
}


void checkCoreSize(mxProc *c, int fd)
{
   static unsigned long maxVMMB = 0;

   if (!maxVMMB)
   {
#if !defined (_LP64)
      maxVMMB = 4096;
#else
      Elf_Addr pCoreVmLimit = getSymbolAddress(c,"lPMXVmLimit");
      if (pCoreVmLimit)
      {
         maxVMMB = (unsigned long) read_long(c, pCoreVmLimit);
         debug("Read %luB VM Limit from core", maxVMMB);
         if (!maxVMMB)
         {
            debug("VM limit of 0B doesnt make sense. Assuming 8GB");
            maxVMMB = 8192;
         }
         else
         {
            maxVMMB /= (1024*1024); // Convert to MB
         }
      }
      else
      {
         debug("lPMXVmLimit symbol not found.  Assuming 8GB");
         maxVMMB = 8192; // Default ulimit in 64bit mx is 8GB if /LIMIT_SIZE isn't set
      }
#endif
   }

   unsigned long vmSize=0;
   unsigned long fileSize=0;

   for (int i = 0; i < c->elfFile[fd].phs.nph; i++)
   {
      const Elf_Phdr *ph = c->elfFile[fd].phs.ph+i;

      if (ph->p_type == PT_LOAD)
      {
         vmSize += ph->p_memsz;
         fileSize += ph->p_filesz;

         static int warnedHeader=0;
         if (ph->p_offset + ph->p_filesz > c->elfFile[fd].stat.st_size && !warnedHeader++)
            warning("One or more program headers point beyond the end of the file.  Is %s truncated?", c->elfFile[fd].fileName);

#if defined(__sun)
         static int warnedFailure=0;
         if ((ph->p_flags & PF_SUNW_FAILURE) && (ph->p_flags & PF_R) && !warnedFailure++)
            warning("%s is missing data.  Check disk space, ulimit and coreadm.", c->elfFile[fd].fileName);
#endif
      }
   }

   debug("%s defines %lu of vm space of which %lu is mapped to the file. It is %lu bytes in size",  c->elfFile[fd].fileName, vmSize, fileSize, c->elfFile[fd].stat.st_size);

   if (fileSize > c->elfFile[fd].stat.st_size)
      warning("%s defines %lu bytes of data but is only %lu bytes in size.  The file appears to be truncated.", c->elfFile[fd].fileName, fileSize, c->elfFile[fd].stat.st_size);

   if (vmSize >= (maxVMMB-512)*1024*1024) // Warn within 512MB of limit
      warning("%s contains %dMB of virtual memory space, and has possibly hit the %dMB limit.",c->elfFile[fd].fileName,vmSize/(1024*1024),maxVMMB);

}

int charcompare(const void* a, const void* b)
{
   const char *da = *(const char **)a;
   const char *db = *(const char **)b;
   return strcmp(da, db);
}

void printpmap(mxProc *c)
{
   size_t maxEntries = 10240;
   size_t currentEntry = 0;

   char **entries = reinterpret_cast<char**>(malloc(maxEntries * sizeof(char*)));

   for (int fd = 0 ; fd < c->elfOpen; fd++)
   {
      for (int i = 0; i < c->elfFile[fd].phs.nph; i++)
      {
         const Elf_Phdr *ph = c->elfFile[fd].phs.ph+i;

         if (ph->p_type == PT_LOAD)
         {
            char entry[1024];
            Elf_Addr baseAddr = c->elfFile[fd].phs.baseAddr;
            snprintf(entry, 1024, FMT_ADR "\t %4luk(%4luk)\t", (unsigned long)(baseAddr + ph->p_vaddr),
                     (unsigned long)(ph->p_memsz / 1024),
                     (unsigned long)(ph->p_filesz / 1024));
            if (ph->p_flags & PF_R)
               strcat(entry,"r");
            else
               strcat(entry,"-");

            if (ph->p_flags & PF_W)
               strcat(entry,"w");
            else
               strcat(entry,"-");

            if (ph->p_flags & PF_X)
               strcat(entry,"x");
            else
               strcat(entry,"-");

            strcat(entry,"\t");
            strcat(entry,c->elfFile[fd].fileName);

            if (currentEntry == maxEntries )
            {
               maxEntries+=maxEntries;
               entries = reinterpret_cast<char**>(realloc(entries, maxEntries * sizeof(char*)));
               debug("doubling pmap entries to %d\n", maxEntries);
            }

            entries[currentEntry++] = strdup(entry);
         }
      }
   }

   // Sort entries
   qsort(entries, currentEntry, sizeof(char*), charcompare);

   if (c->type == mxProcTypePID)
   {
      printf("Warning: Only memory segments from binary files are printed for live processes.\n");
      printf("         For a full list of mappings, run:\n");
#if defined(__linux)
      printf("cat /proc/%d/maps\n\n", c->pid);
#elif defined(__sun)
      printf("pmap %d\n\n", c->pid);
#endif
   }
   else
   {
      printf("Note: As some memory segments are available multiple locations, it is expected\n");
      printf("      to see duplicates below.  e.g. code segments may be available in both\n");
      printf("      and the binary and core file\n\n");
   }

   // Print entries
   printf("Address\tSize(available in file)\tFlags\tFilename\n");
   for (size_t i=0; i<currentEntry; i++)
   {
      printf("%s\n", entries[i]);
      free(entries[i]);
   }

   free(entries);
   printf("\n");

}

int openElfFile(mxProc *c,  const char *fileName, Elf_Addr baseAddr, int justHeaders, int failIfInvalid)
{
   int intFD = c->elfOpen++;

   // We can't mmap the whole thing as it won't fit in our address space for large files in 32bit.

   // first just mmap the main header so we can get the total size of the headers
   size_t toMapSize = 0;
   if (justHeaders)
      toMapSize = sizeof(Elf_Ehdr);

   c->elfFile[intFD].mmloc = static_cast <void *>(mmapFile(c, fileName, &c->elfFile[intFD].fd, &toMapSize, &c->elfFile[intFD].stat));
   debug("Opening Elf File [%s] with internal elfID %d, FD %d, bytes %ld, offset " FMT_ADR,fileName,intFD,c->elfFile[intFD].fd,toMapSize,baseAddr);
   c->elfFile[intFD].mmsize = toMapSize;

   // Read ELF File header and confirm it's a valid file
   const Elf_Ehdr *hdrCore = reinterpret_cast < const Elf_Ehdr * >(c->elfFile[intFD].mmloc);
   if (!isValidElf(c, hdrCore))
   {
      if (failIfInvalid)
      {
         fatal_error("Unable to open %s as it isn't a valid ELF file, or isn't supported by this version of pmx.", fileName);
      }
      else
      {
         // Unmap the file and return 0;
#ifdef __sun
         munmap(reinterpret_cast<char *>(c->elfFile[intFD].mmloc),sizeof(Elf_Ehdr));
#else
         munmap(c->elfFile[intFD].mmloc,sizeof(Elf_Ehdr));
#endif
         c->elfFile[intFD].mmloc=0;
         c->elfFile[intFD].mmsize=0;
         c->elfFile[intFD].fd=0;
         c->elfOpen--;
         return 0;
      }
   }


   // Warn if newer than the core file (0)
   if ( c->type == mxProcTypeCore && intFD && difftime(c->elfFile[intFD].stat.st_mtime,c->elfFile[0].stat.st_mtime) > 0 )
      warning("%s is newer than %s.  Has the environment been upgraded?",fileName, c->elfFile[0].fileName);

   if (justHeaders)
   {
      //Remap to include program and section headers
      toMapSize=hdrCore->e_shoff + hdrCore->e_shnum * sizeof(Elf_Shdr);
      if (toMapSize<hdrCore->e_phoff + hdrCore->e_phnum * sizeof(Elf_Phdr))
         toMapSize=hdrCore->e_phoff + hdrCore->e_phnum * sizeof(Elf_Phdr);

      //Solaris has a bug in the munmap definition
#ifdef __sun
      munmap(reinterpret_cast<char *>(c->elfFile[intFD].mmloc),sizeof(Elf_Ehdr));
#else
      munmap(c->elfFile[intFD].mmloc,sizeof(Elf_Ehdr));
#endif

      c->elfFile[intFD].mmloc = static_cast <void *>(mmapFile(c, fileName, &c->elfFile[intFD].fd, &toMapSize, &c->elfFile[intFD].stat));
      c->elfFile[intFD].mmsize = toMapSize;
      debug("Reopening Elf File [%s] with internal elfID %d, FD %d, bytes %ld",fileName,intFD,c->elfFile[intFD].fd,toMapSize);
   }

   hdrCore = reinterpret_cast < const Elf_Ehdr * >(c->elfFile[intFD].mmloc);

   // Read program
   c->elfFile[intFD].phs.nph = hdrCore->e_phnum;
   c->elfFile[intFD].phs.ph = reinterpret_cast < const Elf_Phdr *>((Elf_Addr)c->elfFile[intFD].mmloc + hdrCore->e_phoff);
   c->elfFile[intFD].phs.baseAddr = baseAddr;

   c->elfFile[intFD].fileName = (char *) malloc(strlen(fileName)+1);
   strcpy(c->elfFile[intFD].fileName, fileName);


   return intFD;
}

mxProc *openCoreFile(const char *binFileName, const char *coreFileName, const char *libraryRoot, int plddMode)
{
   mxProc *c = static_cast < mxProc * >(malloc(sizeof(mxProc)));

   initMxProc(c);
   c->type = mxProcTypeCore;

   // We don't load the symbols from the core as it's problematic
   openElfFile(c, coreFileName, 0, 1, 1);

   getLWPsFromCore(c);

   if (binFileName == NULL && c->binFile[0])
      binFileName = c->binFile;
   else if (binFileName==NULL)
      binFileName = "mx";

   if (strcmp(coreFileName,"core")==0 && c->pid)
      snprintf(c->filePrefix,sizeof(c->filePrefix),"core_%d",c->pid);
   else
      strncpy(c->filePrefix,coreFileName,sizeof(c->filePrefix));

   int binFileID = openElfFile(c, binFileName, 0, 0, 1);
   loadSymbols(c, binFileID, 0);
   checkCoreSize(c,0);
   loadLibraries(c, binFileID, libraryRoot, plddMode);

   return c;
}

void getFileAddrFromCore(const mxProc *c, Elf_Addr vmAddr, Elf_Addr *fileAddr, int *elfFile, int so)
{
   *fileAddr=0;

   if (so != FILEONLY)
      *elfFile=0;

   for (int j = 0 ; j < c->elfOpen; j++)
   {
      // Normally we want to find the core (file 0) first as that has the data from the time of the crash
      // However, if we are actually interested in the file from where the data was originally loaded
      // We should go backwards so we match the library before the core.
      int fd;
      if (so == CORELAST)
      {
         fd=c->elfOpen-j;
      }
      else if (so == FILEONLY) // Only search the file passed in as elfFile
      {
         fd=*elfFile;
         *elfFile=0;
      }
      else // COREONLY or COREFIRST
      {
         fd=j;
      }

      for (int i = 0; i < c->elfFile[fd].phs.nph; i++)
      {
         const Elf_Phdr *ph = c->elfFile[fd].phs.ph+i;
         if (ph->p_type == PT_LOAD && ph->p_filesz)
         {
            Elf_Addr baseAddr = c->elfFile[fd].phs.baseAddr;
            if (ph->p_memsz < ph->p_filesz)
            {
               // This shouldn't happen
               warning("Program Header %d file size (%#lx) is bigger than memory size (%#lx).  Ignoring.",
                       i, (unsigned long) ph->p_filesz, (unsigned long) ph->p_memsz);
            }
            else if (vmAddr >= baseAddr + (Elf_Addr) ph->p_vaddr && vmAddr < baseAddr + (Elf_Addr) ph->p_vaddr + ph->p_filesz)
            {
               *fileAddr = ph->p_offset + vmAddr - ph->p_vaddr - baseAddr;
               *elfFile = fd;
               return;
            }
            else if (vmAddr >= baseAddr + (Elf_Addr) ph->p_vaddr && vmAddr < baseAddr + (Elf_Addr) ph->p_vaddr + ph->p_memsz)
            {
               // Record an incomplete section, but keep looking case we have a full
               // section in one of the other elf files (e.g. sometimes the core has just the first 4k of a LOAD)
               *fileAddr = ADDR_NULLVALUES;
               *elfFile = fd;
            }
         }
      }

      if (so == COREONLY || so == FILEONLY)
         return;
   }
}

static const char *searchSymbolTable(const mxSymTab_t * t, Elf_Addr vmAddr, Elf_Off * offset)
{
   const Elf_Sym *symbol = t->table;
   int items = t->size / sizeof(Elf_Sym);
   int i = 0;

   for (i = 0; i < items; i++, symbol++)
   {
      if (symbol->st_size && symbol->st_value && symbol->st_shndx)
      {
         //debug("found symbol " FMT_ADR " sized " FMT_ADR ", named %s\n", symbol->st_value, symbol->st_size, t->strings+symbol->st_name);
         if (vmAddr >= t->baseAddr + symbol->st_value && vmAddr < t->baseAddr + symbol->st_value + symbol->st_size)
         {
            *offset = vmAddr - t->baseAddr - symbol->st_value;
            return t->strings + symbol->st_name;
         }
      }
   }

   return 0;
}

static Elf_Addr searchSymbolTable(const mxSymTab_t * t, const char *symbolName)
{
   const Elf_Sym *symbol = t->table;
   int items = t->size / sizeof(Elf_Sym);
   int i = 0;
   char demangled[10240];

   for (i = 0; i < items; i++, symbol++)
   {
      const char *mangled = t->strings + symbol->st_name;

      if (! strstr(mangled, symbolName))
         continue;

      demangleSymbolName(t->strings + symbol->st_name, demangled, sizeof(demangled));

      // Strip off arguments
      char *firstpar = strstr(demangled,"(");
      if (firstpar)
         firstpar[0]='\0';

      if (strcmp(demangled, symbolName)==0)
      {
         //debug("Found Symbol [%s] as item %d with value " FMT_ADR " in table with offset " FMT_ADR, symbolName, i,            symbol->st_value,t->baseAddr);
         return t->baseAddr + symbol->st_value;
      }
   }

   return 0;
}

const char *getUnknownSymbol()
{
   return unknownSymbol;
}

static const char *getSymbolName(const mxProc * c, Elf_Addr vmAddr, Elf_Off * offset)
{
   int i;

   *offset = 0;
   const char *symbolName = NULL;

   for (i = 0; i < c->nsymtabs; i++)
   {
      symbolName = searchSymbolTable(c->symtab + i, vmAddr, offset);
      if (symbolName && symbolName[0])
         return symbolName;
   }

   return getUnknownSymbol();
}

static const char *getFileName(const mxProc *c, Elf_Addr vmAddr)
{
   Elf_Addr fileAddr = 0;
   int elfFile = 0;
   //Search the core last, as sometimes the binary code is there.  We are interested in the originating file
   getFileAddrFromCore(c, vmAddr, &fileAddr, &elfFile, CORELAST);
   return c->elfFile[elfFile].fileName;
}


Elf_Addr getSymbolAddress(const mxProc * c, const char *symbolName)
{
   int i;

   Elf_Addr symbolAddress = 0;

   for (i = 0; i < c->nsymtabs; i++)
   {
      //debug("Testing symbol table %d with offset " FMT_ADR, i, c->symtab[i].baseAddr);
      symbolAddress = searchSymbolTable(c->symtab + i, symbolName);
      if (symbolAddress)
         return symbolAddress;
   }

   return 0;
}

static const char *anonNamespace = "(anonymous namespace)";

char *get_short_function_name(char *name)
{
   // Gets a pointer to the 'method' part of a function.
   // e.g. murex::namespace::class::mymethod will return a pointer to mymethod
   char *c=name + strlen(name);
   while (c > name)
   {
      if (*c == ':')
         return c+1;

      c--;
   }

   return c;
}

char *get_function_name_from_prototype(char *name)
{
   // determine the name of the function by finding the first '('
   // character, (except if it is '(anonymous namespace)'
   // and then scan backwards until we reach either a space
   // or the start of the string or *
   char * result= (char *) malloc(strlen(name) + 1);
   char * parenthesis;
   char * w;

   parenthesis = strchr(name, '(');

   if (parenthesis && strncmp(parenthesis,anonNamespace,strlen(anonNamespace))==0)
      parenthesis = strchr(parenthesis+strlen(anonNamespace), '(');

   if (parenthesis == NULL)
      parenthesis = name + strlen(name); // No parenthesis, function name ends on last character of string

   w = parenthesis - 1 ; // last function name is before parenthesis.
   int level=0;
   while (w >= name && (level || (*w != '*' && *w != ' ')))
   {
      if (*w == '>' || *w == ')')
         level++;
      else if (*w == '<' || *w == '(')
         level--;

      w--;
   }
   w++; //w points before the function name start, set it to first char of function name
   memmove(result, w, parenthesis - w);
   result[parenthesis - w] = '\0';
   return result;
}

int adjustArgsForCppMethod(char *name, mxArguments *args)
{
   char *functionName = get_function_name_from_prototype(name);
   if (strstr(functionName,"::"))
   {
      // Shift all arguments forward by 1 to allow for 'this'
      for (int i=args->count; i>0; i--)
         args->arg[i].type = args->arg[i-1].type;
      args->count++;

      // This is a C++ class method.  Strip off the method and add the class name as the first argument as a class pointer
      for (char *p = functionName + strlen(functionName); p > functionName; p--)
      {
         if (strstr(p,"::"))
         {
            *(p++)='*';
            *p='\0';
            break;
         }
      }
      debug("adding arg 0 as C++ class %s", functionName);
      args->arg[0].type=functionName;
      return 1;
   }
   else
   {
      free(functionName);
      return 0;
   }
}

int get_arg_types_from_prototype(char *name, mxArguments *args)
{
   // No point trying if we don't have a symbol
   if (name == getUnknownSymbol())
      return 0;

   debug("getting args from: %s", name );

   char *w = strchr(name, '(');

   // Ignore anonNamespace
   if (w && strncmp(w,anonNamespace,strlen(anonNamespace))==0)
      w = strchr(w+strlen(anonNamespace), '(');

   if (!w)
   {
      debug("could not find args in : %s", name);
      return 0;
   }

   debug("found args in : %s", name);

   int arg_index = 0;

   w++; // skip '('
   int level = 0;
   while (*w && *w != ')')
   {
      args->arg[arg_index].type = (char *) calloc(strlen(name), sizeof(char));
      char *arg_ptr = args->arg[arg_index].type;
      debug("starting arg %d", arg_index);
      while(*w && ((level > 0) || (*w != ')' && *w != ',')))
      {
         if (*w == '(' || *w == '<')
         {
            level ++;
            debug("getting deeper %d", level);
         }
         else if (*w == ')' || *w == '>')
         {
            level --;
            debug("going up %d", level);
         }
         else if (*w == ' ') // Exclude spaces
         {
            w++;
            continue;
         }
         else if (!strncmp(w,"const",5)) // Exclude const keyword
         {
            w+=5;
            continue;
         }

         *(arg_ptr++) = *(w++);
      }
      *(arg_ptr++) = '\0';
      debug("got arg %s", args->arg[arg_index].type);
      if (*w == ',' || *w == ')')
      {
         arg_index++;
         w++;
      }

   }

   args->count=arg_index;

   return 1;
}

void getInstrumentedArguments(const mxProc *proc, Elf_Addr frameAddr, int verbose, mxArguments *args)
{
#if !defined(__sparc)
   static Elf_Addr lastFoundTag = 0; // To make sure we don't find the tag from the last frame
   unsigned long l;

   Elf_Addr addrStartTag, addrEndTag;
   for (addrEndTag = frameAddr; addrEndTag>lastFoundTag && addrEndTag > frameAddr-100*sizeof(Elf_Addr) ; addrEndTag-=sizeof(Elf_Addr))
   {
      readMxProcVM(proc,addrEndTag,&l,sizeof(l));
      if (l ==  PMX_INSTRUMENT_END_TAG)
      {
         debug("PMX Instrumentation: End tag at " FMT_ADR "", addrEndTag);
         break;
      }
   }

   if (l ==  PMX_INSTRUMENT_END_TAG && addrEndTag != lastFoundTag)
   {
      for (addrStartTag = addrEndTag; addrStartTag>lastFoundTag && addrStartTag > addrEndTag-20*sizeof(Elf_Addr) ; addrStartTag-=sizeof(Elf_Addr))
      {
         readMxProcVM(proc,addrStartTag,&l,sizeof(l));
         if (l ==  PMX_INSTRUMENT_START_TAG)
         {
            debug("PMX Instrumentation: Start tag at " FMT_ADR, addrStartTag);
      		lastFoundTag = addrEndTag;
            break;
         }
      }
      if (l != PMX_INSTRUMENT_START_TAG)
      {
         debug("PMX Instrumentation: Failed to find start tag");
         addrEndTag = addrStartTag = 0;
      }

   }
   else
   {
      debug("PMX Instrumentation: Failed to find instumentation data");
      addrEndTag = addrStartTag = 0;
   }

   if (addrStartTag)
   {
      int i=0;
      for (Elf_Addr addrArg = addrStartTag + sizeof(Elf_Addr); addrArg < addrEndTag ; addrArg +=sizeof(Elf_Addr))
         addInstrumentedArgument(proc, args, i++, addrArg, sizeof(Elf_Addr));
   }
#endif
}

void printStackItem(const mxProc * p, Elf_Addr addr, Elf_Addr frameAddr, int fullStack, int stackArguments)
{
   char demangled[10240] = "";
   Elf_Off symbolOffset = 0;
   char *functionName = NULL;
   int cppMethod=0;
   int foundArguments=0;
   char symbolType='U'; // 'U'nknown
   // 'F'unction (includes static method)
   // 'M'ethod

   const char *symbolName = getSymbolName(p, addr, &symbolOffset);

   if (symbolName == getUnknownSymbol())
   {
      strncpy(demangled, symbolName, sizeof(demangled));
   }
   else
   {
      demangleSymbolName(symbolName, demangled, sizeof(demangled));
      functionName = get_function_name_from_prototype(demangled);

      char * shortFunctionName = get_short_function_name(functionName);
#ifdef __sun
      // Look at the mangled symbol to try to determine if we are a standard function (including static method) or a real method
      // Search for the funciton name in the mangled symbol followed by the number 6, then get the letter following it
      char mangledFunction[10240];
      sprintf(mangledFunction,"%s6",shortFunctionName);
      const char *c = strstr(symbolName,mangledFunction);
      if (c)
      {
         c+=strlen(mangledFunction);

         // Sometimes we see the k flag first.  We should skip it.
         if (*c=='k')
            c++;

         if (*c =='M' || *c == 'F')
         {
            debug("Found method type %c in demangled symbol", *c);
            symbolType=*c;
         }
         else
         {
            debug("Found unknown symbol flag %c", *c);
         }
      }
#endif
      // operators are methods
      if (symbolType=='U' && !strncmp(shortFunctionName,"operator",8))
      {
         symbolType='M';
      }

   }

   // The maximum number of arguments we can expect in the type specific arrays
   int maxInt=0;
   int maxFloat=0;
#if defined (_LP64) && (__x86_64)
   maxInt=6;
   maxFloat=4;
#endif

   // Get Argument values from stack.  These are loaded into args->intArg[] and args->floatArg[]
   mxArguments *args = getArguments(p,addr-symbolOffset,frameAddr,0);

   // Get instrumented arguments
   getInstrumentedArguments(p,frameAddr,0,args);

   // Get Argument types from demangled prototype.  These are loaded into args->arg[]
   if (symbolName != getUnknownSymbol())
   {
      foundArguments = get_arg_types_from_prototype(demangled, args);

      // If we are a C++ method, we need an extra argument for 'this'
      if (symbolType == 'U' && args->intCount >= maxInt )
      {
         // If we have 6 or more arguments intCount is probably inaccurate as we will either be
         // 32bit where we just read a block of 8 arguments, or we are 64bit with extra args passed on the stack
         // We will assume we are a method and print a warning
         cppMethod=adjustArgsForCppMethod(demangled, args);

         if (cppMethod && fullStack)
            warning("Assuming %s is a C++ method. If this is incorrect, arguments will be offset by 1.", functionName);
         else if (cppMethod && ! fullStack)
            debug("Assuming %s is a C++ method. If this is incorrect, arguments will be offset by 1.", functionName);
      }
      else if (symbolType == 'M' ||
               (symbolType == 'U' && foundArguments && (args->intCount + args->floatCount) == (args->count + 1)))
      {
         cppMethod=adjustArgsForCppMethod(demangled, args);
      }
   }

   debug("Found %d int args, %d float args, %d instrumented args and %d prototype args for [%s] which is at " FMT_ADR " with a frame at " FMT_ADR,
         args->intCount, args->floatCount, args->instCount, args->count, demangled,addr-symbolOffset,frameAddr);


   // Now update args->arg based on the prototype if available, copying values from intArg and floatArg
   if (args->instCount)
   {
      if (args->count != args->instCount)
         warning("Instrumented argument count (%d) doesn't match prototype (%d)", args->instCount, args->count);

      for (int i=0; i<args->count && i<args->instCount; i++)
      {
         debug("copying instrumented arg %d to overall arg %d for type %s", i, i, args->arg[i].type);
         args->arg[i].size = args->instArg[i].size;
         args->arg[i].val = args->instArg[i].val;
      }

   }
   else if (foundArguments)
   {
      int iInt=0;
      int iFloat=0;
      int iStack=0;

      for (int i=0; i<args->count; i++)
      {
         if (strcmp(args->arg[i].type,"double")==0)
         {
            if (iFloat < args->floatCount)
            {
               debug("copying float arg %d to overall arg %d for type %s", iFloat, i, args->arg[i].type);
               args->arg[i].size = args->floatArg[iFloat].size;
               args->arg[i].val = args->floatArg[iFloat].val;
            }
            else if (iFloat >= maxFloat && iStack+1 < args->stackCount)
            {
#if defined (_LP64)
               debug("copying stack arg %d to overall arg %d for type %s", iStack, i, args->arg[i].type);
               args->arg[i].val = args->stackArg[iStack].val;
#else
               debug("copying stack args %d and %d to overall arg %d for type %s", iStack, iStack+1, i, args->arg[i].type);
               // 32bit takes 2 ints to make a double.  Copy by byte to avoid strange casting issue.
               args->arg[i].val.valChar[0] = args->stackArg[iStack].val.valChar[0];
               args->arg[i].val.valChar[1] = args->stackArg[iStack].val.valChar[1];
               args->arg[i].val.valChar[2] = args->stackArg[iStack].val.valChar[2];
               args->arg[i].val.valChar[3] = args->stackArg[iStack].val.valChar[3];
               iStack++;
               args->arg[i].val.valChar[4] = args->stackArg[iStack].val.valChar[0];
               args->arg[i].val.valChar[5] = args->stackArg[iStack].val.valChar[1];
               args->arg[i].val.valChar[6] = args->stackArg[iStack].val.valChar[2];
               args->arg[i].val.valChar[7] = args->stackArg[iStack].val.valChar[3];
#endif
               args->arg[i].size = 8; // double is always 8 bytes
               iStack++;
            }
            else
            {
               debug("Not enough float arguments to match prototype.  We only have %d", args->floatCount);
            }

            iFloat++;
         }
         else
         {
            if (iInt < args->intCount)
            {
               debug("copying int arg %d to overall arg %d for type %s", iInt, i, args->arg[i].type);
               args->arg[i].size = args->intArg[iInt].size;
               args->arg[i].val = args->intArg[iInt].val;
            }
            else if (iInt >= maxInt && iStack < args->stackCount)
            {
               debug("copying stack arg %d to overall arg %d for type %s", iStack, i, args->arg[i].type);
               args->arg[i].size = args->stackArg[iStack].size;
               args->arg[i].val = args->stackArg[iStack].val;
               iStack++;
            }
            else
            {
               debug("Not enough int arguments to match prototype.  We only have %d", args->intCount);
            }

            iInt++;
         }
      }
   }
   else
   {
      // No Arguments from prototype.  Put int arguments first, then floats
      if (args->floatCount && fullStack)
         warning("Unable to correctly order arguments.  Integer types will be displayed first followed by floating point types"); //TODO error not accurate

      int i=0;

      for (int iInt=0; iInt<args->intCount; iInt++)
      {
         args->arg[i].size = args->intArg[iInt].size;
         args->arg[i].val = args->intArg[iInt].val;
         args->arg[i].type = strdup(getUnknownSymbol());
         i++;
      }

      for (int iFloat=0; iFloat<args->floatCount; iFloat++)
      {
         args->arg[i].size = args->floatArg[iFloat].size;
         args->arg[i].val = args->floatArg[iFloat].val;
         args->arg[i].type = strdup(getUnknownSymbol());
         i++;
      }

      if (args->intCount >= maxInt || args->floatCount >= maxFloat)
      {
         for (int iStack=0; i < stackArguments && iStack < args->stackCount; iStack++)
         {
            args->arg[i].size = args->stackArg[iStack].size;
            args->arg[i].val = args->stackArg[iStack].val;
            args->arg[i].type = strdup(getUnknownSymbol());
            i++;
         }
      }

      args->count=i;
   }

   if (fullStack)
   {
      printf(FMT_ADR " %s(", (unsigned long) addr, demangled);

      for (int i = 0; i < args->count; i++)
      {
         if (cppMethod && i==0)
            printf("this=");

         if (args->arg[i].size == 0)
         {
            printf("<unknown>");
         }
         else if (strcmp(args->arg[i].type,"double")==0)
         {
            printf("%f", args->arg[i].val.valDouble);
         }
         else
         {
            switch (args->arg[i].size)
            {
               case 8: printf("%#lx", args->arg[i].val.val); break;
               case 4: printf("%#x",  args->arg[i].val.val4); break;
               case 1: printf("%#hhx", args->arg[i].val.val1); break;
            }
         }

         if (cppMethod && i==0)
            printf("; ");
         else if (i != args->count - 1)
            printf(", ");
      }

      printf(") + %#lx [%s]\n", (unsigned long) symbolOffset, getFileName(p,addr));
   }


   // Print argument analysis if we have a symbol
   if (symbolName != getUnknownSymbol())
      display_arguments(p, functionName, args);

   if (functionName)
      free(functionName);

   for (int i=0; i <args->count; i++)
      free(args->arg[i].type);

   free(args);
}

// Handles both PID & Cores
void closeMxProc(mxProc * p)
{
   // Close and unmm all symtable files

   while (p->elfOpen)
   {
      free(p->elfFile[p->elfOpen-1].fileName);
      close(p->elfFile[p->elfOpen-1].fd);
      p->elfOpen--;
   }
   if (p->type == mxProcTypePID)
   {
      closeMxProcPID(p);
   }

   free(p);
}

void dumpStack(const mxProc * p, mxLWP_t t, int words)
{
   size_t bias = 0;
#if (defined(__sparc) && defined (_LP64))
   bias = 0x7ff;
#endif

   printf("Dumping Stack sp:" FMT_ADR " fp:" FMT_ADR " ip:" FMT_ADR " stack:" FMT_ADR " size:" FMT_ADR "\n", (unsigned long) t.sp, (unsigned long) t.fp, (unsigned long) t.ip,
          (unsigned long) t.stack, (unsigned long) t.stacksize);

   if (bias)
   {
      t.sp+=bias;
      t.fp+=bias;
      printf("Applying stack bias:" FMT_ADR " New sp: " FMT_ADR " New fp: " FMT_ADR "\n", (unsigned long)bias, (unsigned long) t.sp, (unsigned long) t.fp);
   }

   Elf_Addr currentValue;
   size_t i = 0;
   size_t maxSize = t.stack ? t.stacksize / sizeof(void*) : words;
   char demangled[10240];
   Elf_Addr nextFrame=t.fp;
   debug("Printing %ld words of stack",maxSize);


   for (i = 0; i < maxSize; i++)
   {
      Elf_Addr currentAddress = t.sp + i * sizeof(void *);

      readMxProcVM(p, currentAddress, &currentValue, sizeof(void *));

      Elf_Off symbolOffset = 0;
      if ((currentValue+bias) > t.sp && (currentValue+bias) < t.sp + maxSize * sizeof(void *))
      {
         strcpy(demangled, "STACKPOINTER");
         symbolOffset = (currentValue+bias) - t.sp;
      }
      else
      {
         const char *symbolName = getSymbolName(p, currentValue, &symbolOffset);

         demangleSymbolName(symbolName, demangled, sizeof(demangled));
      }
      printf("%d   " FMT_ADR ": " FMT_ADR " == %s + %#lx (%ld) [%s]", (int) (i * sizeof(void *)), (unsigned long)currentAddress, (unsigned long) currentValue,
             demangled, (unsigned long) symbolOffset, (unsigned long) symbolOffset, getFileName(p,currentValue));

      if (currentAddress == nextFrame)
      {
         printf(" ******** FRAME ********");
#if !defined(__sparc)
         nextFrame=currentValue;
#endif
      }

#if defined(__sparc)
      if (currentAddress == nextFrame + 14 * sizeof(void*))
      {
         nextFrame=currentValue+bias;
         printf(" ******** Next Frame is " FMT_ADR " ******** ", nextFrame);
      }
#endif

      printf("\n");
   }
}

int read_int(const mxProc * p, Elf_Addr vmAddr)
{
   int ret=0;

   if (readMxProcVM(p, vmAddr, &ret, sizeof(ret)))
      warning("Unable to read int");

   return ret;
}

long read_long(const mxProc * p, Elf_Addr vmAddr)
{
   long ret=0;

   if (readMxProcVM(p, vmAddr, &ret, sizeof(ret)))
      warning("Unable to read long");

   return ret;
}

Elf_Addr read_addr(const mxProc * p, Elf_Addr vmAddr)
{
   Elf_Addr ret=0;

   if (readMxProcVM(p, vmAddr, &ret, sizeof(ret)))
      warning("Unable to read address");

   return ret;
}

double read_double(const mxProc * p, Elf_Addr vmAddr)
{
   double ret=0.0;

   if (readMxProcVM(p, vmAddr, &ret, sizeof(ret)))
      warning("Unable to read double");

   return ret;
}

void read_string(const mxProc * p, Elf_Addr vmAddr, char *buf, size_t buf_length)
{

   // First try reading chunks which is faster
   size_t i=0;
   const size_t chunkSize=1024;
   while (buf_length - i >= chunkSize)
   {
      char c[chunkSize];
      if (readMxProcVM(p, vmAddr+i, &c, chunkSize))
      {
         // This isn't necessarily an issue. Retry going one byte at a time.
         break;
      }
      else
      {
         memcpy(buf+i, c, chunkSize);

         // If there is a null inside the chunk, stop
         for (int j=0; j< chunkSize; j++)
            if (c[j]=='\0')
               return;
      }

      i+=chunkSize;
   }

   // If we have less than one chunk to go, just iterate one byte at a time
   for (; i<buf_length; i++)
   {
      char c;
      if (readMxProcVM(p, vmAddr+i, &c, 1))
      {
         warning("Failed to read full string");
         buf[i]='\0';
         return;
      }
      else
      {
         buf[i]=c;
         if (c=='\0')
            return;
      }
   }

   //Pad with null to avoid issues
   buf[buf_length-1]='\0';

   return;
}

void read_bytes(const mxProc * p, Elf_Addr vmAddr, void *buf, size_t buf_length)
{
   if (readMxProcVM(p, vmAddr, buf, buf_length))
      warning("Failed to read %lu bytes", buf_length);

   return;
}

void addIntArgument(const mxProc *proc, mxArguments *args, int argNumber, Elf_Addr argAddr, int argLength)
{
   if (args->intCount < argNumber+1)
      args->intCount=argNumber+1;

   args->intArg[argNumber].size=argLength;

   if (readMxProcVM(proc,argAddr,&(args->intArg[argNumber].val.val4),argLength))
   {
      debug("Error reading argument %d",argNumber);
   }
   debug("Added Int  Argument %d size %d bytes from Address " FMT_ADR " with value " FMT_ADR,argNumber,argLength,argAddr,args->intArg[argNumber].val.val);
}

void addStackArgument(const mxProc *proc, mxArguments *args, int argNumber, Elf_Addr argAddr, int argLength)
{
   if (args->stackCount < argNumber+1)
      args->stackCount=argNumber+1;

   args->stackArg[argNumber].size=argLength;

   if (readMxProcVM(proc,argAddr,&(args->stackArg[argNumber].val.val4),argLength))
   {
      debug("Error reading argument %d",argNumber);
   }
   debug("Added Stack Argument %d size %d bytes from Address " FMT_ADR " with value " FMT_ADR,argNumber,argLength,argAddr,args->stackArg[argNumber].val.val);
}

void addInstrumentedArgument(const mxProc *proc, mxArguments *args, int argNumber, Elf_Addr argAddr, int argLength)
{
   if (args->instCount < argNumber+1)
      args->instCount=argNumber+1;

   args->instArg[argNumber].size=argLength;

   if (readMxProcVM(proc,argAddr,&(args->instArg[argNumber].val.val4),argLength))
   {
      debug("Error reading argument %d",argNumber);
   }
   debug("Added Instrumented Argument %d size %d bytes from Address " FMT_ADR " with value " FMT_ADR,argNumber,argLength,argAddr,args->instArg[argNumber].val.val);
}

void addFloatArgument(const mxProc *proc, mxArguments *args, int argNumber, Elf_Addr argAddr, int argLength)
{
   if (args->floatCount < argNumber+1)
      args->floatCount=argNumber+1;

   args->floatArg[argNumber].size=argLength;

   if (readMxProcVM(proc,argAddr,&(args->floatArg[argNumber].val.val4),argLength))
   {
      debug("Error reading float argument %d",argNumber);
   }
   debug("Added Float Argument %d size %d bytes from Address " FMT_ADR " with value %f",argNumber,argLength,argAddr,args->floatArg[argNumber].val.valDouble);
}

void loadLibraries(mxProc * p, int elfID, const char *libraryRoot, int plddMode)
{
   // Make sure we don't over run the buffer
   if (p->nsymtabs >= MAX_SYMTABS)
      return;

   const char * mmFile = reinterpret_cast <const char *>(p->elfFile[elfID].mmloc);
   const Elf_Ehdr *elfHdr = reinterpret_cast < const Elf_Ehdr * >(mmFile);
   const Elf_Phdr *progHdrs = reinterpret_cast < const Elf_Phdr * >(mmFile + elfHdr->e_phoff);


   /* Look for the depths of mysterious linker headers for r_debug */
   for (int i = 0 ; i < elfHdr->e_phnum; i++) {

      if ((progHdrs[i].p_type != PT_DYNAMIC) || (progHdrs[i].p_offset + progHdrs[i].p_filesz > p->elfFile[elfID].mmsize))
         continue;

      Elf_Addr dyn;
      const Elf_Dyn *dynp;
      for (dyn = 0; dyn < progHdrs[i].p_filesz; dyn += sizeof(Elf_Dyn)) {
         dynp = reinterpret_cast < const Elf_Dyn * >(mmFile + progHdrs[i].p_offset + dyn);

         if (dynp->d_tag != DT_DEBUG)
            continue;

         Elf_Dyn dyno;
         readMxProcVM(p, (Elf_Addr)progHdrs[i].p_vaddr + dyn, &dyno, sizeof(dyno));
         debug("Found r_debug at " FMT_ADR, dyno.d_un.d_ptr);

         struct r_debug rDebug;
         readMxProcVM(p, (Elf_Addr) dyno.d_un.d_ptr, &rDebug, sizeof(rDebug));

         struct link_map *mapAddr;
         struct link_map map;

         if (plddMode)
            printf("Loaded Libraries:\n");

         for (mapAddr = rDebug.r_map; mapAddr;
              mapAddr = map.l_next) {

            readMxProcVM(p, (Elf_Addr)mapAddr, &map, sizeof(map));

            if (map.l_name == 0)
               continue;

            char path[1024];
            if (! readMxProcVM(p, (Elf_Addr)map.l_name, path, 1)) // Try to read one byte just to check this is a valid address
               read_string(p, (Elf_Addr)map.l_name, path, sizeof(path));

            if (path[0]) {
               if (strcmp(path,p->elfFile[elfID].fileName)==0)
               {
                  debug("Found main binary [%s] listed in r_debug.  Skipping.", path);
                  continue;
               }

               if (plddMode)
                  printf("    %s\n", path);

               //Replace root for absolute paths
               char fullPath[1024];
               if (path[0] == '/')
                  sprintf(fullPath,"%s%s",libraryRoot,path+1);
               else
                  strncpy(fullPath,path,sizeof(fullPath));

               if (access(fullPath,R_OK) != -1) {
                  int libElfID = openElfFile(p,  fullPath, (Elf_Addr) map.l_addr, 0, 0);
                  if (libElfID)
                     loadSymbols(p, libElfID, (Elf_Addr) map.l_addr);
                  else
                     warning("Unable to open [%s].  Symbols will be unavailable.",fullPath);
               }
               else {
                  warning("Unable to open [%s].  Symbols will be unavailable.",fullPath);
               }
            }
         }

         if (plddMode)
            printf("\n");
      }
   }
}

int readFileByVMAddress(const mxProc * p, Elf_Addr vmAddr, void *buff, size_t size, int elfFile)
{
   // a modified version of readMxProcVm which only looks at a single file

   memset(buff, 0, size);       // Calling functions should check the return value, but in case they dont.....

   // Find file address
   Elf_Addr fileAddr = 0;
   getFileAddrFromCore(p, vmAddr, &fileAddr, &elfFile, FILEONLY);
   //debug("found file address " FMT_ADR " for " FMT_ADR,fileAddr,vmAddr);

   if (!fileAddr)
   {
      debug("Could not find vm address " FMT_ADR " in file %d.  Check pmap.", vmAddr, elfFile);
      return 1;
   }

   Elf_Addr endAddr = 0;
   getFileAddrFromCore(p, vmAddr + size - 1, &endAddr, &elfFile, FILEONLY);

   if (!endAddr)
   {
      debug("Memory address goes beyond mapped areas (base " FMT_ADR " + size %#lx).  Check pmap.", vmAddr, size);
      return 1;
   }

   if (fileAddr == ADDR_NULLVALUES && endAddr == ADDR_NULLVALUES)
   {
      // Valid, but unused memory space.  Return null values.
      return 0;
   }
   else if (fileAddr == ADDR_NULLVALUES || endAddr == ADDR_NULLVALUES)
   {
      warning("Reads across used/unused memory boundaries are not supported (base " FMT_ADR " + size %#lx).", vmAddr, size);
      return 0;
   }

   readFile(p, elfFile, fileAddr, buff, size);

   return 0;
}

void read_string_from_file(const mxProc * p, Elf_Addr vmAddr, char *buf, size_t buf_length, int elfID)
{
   // Cut down version of read_string which only reads from a particular file, and only 1 byte at a time.
   for (size_t i=0; i<buf_length; i++)
   {
      char c;
      if (readFileByVMAddress(p, vmAddr+i, &c, 1, elfID))
      {
         warning("Failed to read full string");
         buf[i]='\0';
         return;
      }
      else
      {
         buf[i]=c;
         if (c=='\0')
            return;
      }
   }

   //Pad with null to avoid issues
   buf[buf_length-1]='\0';

   return;
}

