/*******************************************************************************
*
* Copyright (c) {2003-2018} Murex S.A.S. and its affiliates.
* All rights reserved. This program and the accompanying materials
* are made available under the terms of the Eclipse Public License v1.0
* which accompanies this distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <procfs.h>
#ifdef __GNUC__
#include <cxxabi.h>
#else
#include <demangle.h>
#endif
#include <ucontext.h>

#include "mxProcUtils.h"

void readFile(const mxProc * c, int elfID, Elf_Addr fileAddr, void *buffPointer, size_t size)
{
   if (lseek64(c->elfFile[elfID].fd,fileAddr,SEEK_SET) != fileAddr)
   {
      fatal_error("Failed to seek offset "FMT_ADR" in file %d.",fileAddr, elfID);
   }
   read(c->elfFile[elfID].fd, buffPointer, size);
}

void closeMxProcPID(mxProc * p)
{
   if (p->pid && kill(p->pid, SIGCONT) == -1)
   {
      perror("kill: ");
      printf("Failed to send continue signal to process %ld.\n", (long) p->pid);
   }
   if (p->as)
      close(p->as);
}

void getLWPsFromPID(mxProc * p)
{
   char fileName[128];

   snprintf(fileName, sizeof(fileName), "/proc/%ld/lwp", (long) p->pid);
   struct dirent **namelist;

   int nFiles = scandir(fileName, &namelist, 0, alphasort);
   int t;

   for (t = 0; t < nFiles; t++)
   {
      char *lwpID = namelist[t]->d_name;

      if (lwpID[0] == '.')      // . or ..
         continue;

      snprintf(fileName, sizeof(fileName), "/proc/%ld/lwp/%s/lwpstatus", (long) p->pid, lwpID);

      FILE *f = fopen(fileName, "rbF");

      if (!f)                   // Could be a completed lwp, in which case lwpstatus doesn't exist
         continue;

      lwpstatus_t s;

      if (fread(&s, sizeof(lwpstatus_t), 1, f) != 1)
         fatal_error("Unable to read LWP %s info", lwpID);

      stack_t stackinfo;

      readMxProcVM(p, (Elf_Addr) s.pr_ustack, &stackinfo, sizeof(stack_t));

      prgreg_t *regs = (prgreg_t *) & (s.pr_reg);
      mxLWP_t *lwp = &(p->LWPs[p->nLWPs++]);

      lwp->lwpID = atoi(lwpID);
      lwp->fp = static_cast < Elf_Addr > (regs[R_FP]);
      lwp->sp = static_cast < Elf_Addr > (regs[R_SP]);
      lwp->ip = static_cast < Elf_Addr > (regs[R_PC]);
      lwp->stack = reinterpret_cast < Elf_Addr > (stackinfo.ss_sp);
      lwp->stacksize = stackinfo.ss_size;
      free(namelist[t]);
      if (p->nLWPs >= MAX_LWPS)
         break;
   }
   free(namelist);
}

void getLWPsFromCore(mxProc * c)
{
   int i;

   for (i = 0; i < c->elfFile[0].phs.nph; i++)
   {
      if (c->elfFile[0].phs.ph[i].p_type != PT_NOTE || c->elfFile[0].phs.ph[i].p_filesz == 0)
         continue;

      //printf("program section %d is a note of size %d at offset %x\n",i, c->ph[i].p_filesz, c->ph[i].p_offset);

      int nOff = 0;

      while (nOff < c->elfFile[0].phs.ph[i].p_filesz)
      {
         Elf_Nhdr n;
         Elf_Addr fileAddr = c->elfFile[0].phs.ph[i].p_offset + nOff;
         readFile(c,0,fileAddr,&n,sizeof(n));

         int descOffset = sizeof(Elf_Nhdr) + n.n_namesz;
         while (descOffset % sizeof(int) != 0)
            descOffset++;

         int dataSize = descOffset + n.n_descsz;
         while (dataSize % sizeof(int) != 0)
            dataSize++;

         //printf("Found note [Name Size: %d, Desc Size: %d, Desc Offset: %d, Total Data Size: %d Type: %d ]\n", n->n_namesz, n->n_descsz, descOffset, dataSize, n->n_type);
         if (n.n_type == NT_LWPSTATUS)
         {
            lwpstatus_t s;
            readFile(c, 0, fileAddr+descOffset, &s, sizeof(s));

            stack_t stackinfo;

            readMxProcVM(c, (Elf_Addr) s.pr_ustack, &stackinfo, sizeof(stack_t));

            prgreg_t *regs = (prgreg_t *) & (s.pr_reg);

            mxLWP_t *lwp = &(c->LWPs[c->nLWPs++]);

            lwp->lwpID = s.pr_lwpid;
            lwp->fp = static_cast < Elf_Addr > (regs[R_FP]);
            lwp->sp = static_cast < Elf_Addr > (regs[R_SP]);
            lwp->ip = static_cast < Elf_Addr > (regs[R_PC]);
            lwp->stack = reinterpret_cast < Elf_Addr > (stackinfo.ss_sp);
            lwp->stacksize = stackinfo.ss_size;
         }
         else if (n.n_type == NT_PSINFO)
         {
            psinfo_t s;
            readFile(c, 0, fileAddr+descOffset, &s, sizeof(s));
            c->pid = s.pr_pid;
            // Sometimes s.pr_psargs isn't null terminated so don't run over the end
            printf("Loading core for PID: %d Command: %.*s\n\n",s.pr_pid, (int) sizeof(s.pr_psargs), s.pr_psargs);
            char *endOfBin=strchr(s.pr_psargs, ' ');
            if (endOfBin)
            {
               strncpy(c->binFile, s.pr_psargs, endOfBin - s.pr_psargs);
               debug("Detected binary name [%s]", c->binFile);
            }
            else
            {
               debug("Couldn't extract binary from command line");
            }
         }

         nOff = nOff + dataSize;
         if (c->nLWPs >= MAX_LWPS)
            break;
      }
   }
}

int readMxProcVM(const mxProc * p, Elf_Addr vmAddr, void *buff, size_t size)
{
   memset(buff, 0, size);       // Calling functions should check the return value, but in case they dont.....

   if (p->type == mxProcTypeCore)
   {
      // Find file address
      Elf_Addr fileAddr = 0;
      int elfFile = 0;
      getFileAddrFromCore(p, vmAddr, &fileAddr, &elfFile, COREFIRST);
      //debug("found file address "FMT_ADR" for "FMT_ADR,fileAddr,vmAddr);

      if (!fileAddr)
      {
         debug("Could not find vm address "FMT_ADR" in the core file.  Check pmap.", vmAddr);
         return 1;
      }

      Elf_Addr endAddr = 0;
      int endElfFile = 0;
      getFileAddrFromCore(p, vmAddr + size - 1, &endAddr, &endElfFile, COREFIRST);

      if (!endAddr)
      {
         debug("Memory address goes beyond mapped areas (base "FMT_ADR" + size %#lx).  Check pmap.", vmAddr, size);
         return 1;
      }

      if (elfFile!=endElfFile)
      {
         debug("Memory address "FMT_ADR" goes across 2 elf files.", vmAddr);
         return 1;
      }

      if (fileAddr == ADDR_NULLVALUES && endAddr == ADDR_NULLVALUES)
      {
         // Valid, but unused memory space.  Return null values.
         return 0;
      }
      else if (fileAddr == ADDR_NULLVALUES || endAddr == ADDR_NULLVALUES)
      {
         warning("Reads across used/unused memory boundaries are not supported (base "FMT_ADR" + size %#lx).", vmAddr, size);
         return 0;
      }

      readFile(p, elfFile, fileAddr, buff, size);
   }
   else if (p->type == mxProcTypePID)
   {
      if (pread(p->as, buff, size, (Elf_Off) vmAddr) != size)
      {
         //perror("pread: ");
         debug("Unable to read %d bytes from location "FMT_ADR, size, vmAddr);
         return 1;
      }
   }
   else
   {
      fatal_error("Invalid MxProcType.");
   }
   return 0;
}

void demangleSymbolName(const char *symbolName, char *demangled, int size)
{
   size_t tmp_size = size;
   int status;
   if (symbolName == getUnknownSymbol())
   {
      strcpy(demangled, symbolName);
      return;
   }

   debug("attempting  to demangle %s",symbolName);

#ifdef __GNUC__
   abi::__cxa_demangle(symbolName, demangled, &tmp_size, &status ) ;
#else
   char *tmpBuff = static_cast <char *>(malloc(size));
   status = cplus_demangle(symbolName, tmpBuff, size);
   if(status == 0)
      strncpy(demangled, tmpBuff, size);
   free(tmpBuff);
#endif

   if (status != 0 ) {
      debug("demangle of %s failed",symbolName);
      strcpy(demangled, symbolName);
      return;
   }
}

mxProc *openPID(const char *binFileName, const char *pid, int plddMode)
{
   mxProc *p = static_cast < mxProc * >(malloc(sizeof(mxProc)));

   initMxProc(p);
   p->type = mxProcTypePID;
   sprintf(p->filePrefix,"pmx.pid%s",pid);

   char fileName[1024];

   snprintf(fileName, sizeof(fileName), "/proc/%s/as", pid);

   p->as = open(fileName, O_RDONLY);
   if (p->as == -1)
   {
      perror("open: ");
      fatal_error("Unable to open address space: %s", fileName);
   }

   // Set pid now so that if anything else fails, the cleanup should send SIGCONT
   p->pid = atoi(pid);

   if (kill(p->pid, SIGSTOP) == -1)
   {
      perror("kill: ");
      fatal_error("Failed to send continue signal to process %d.", p->pid);
   }

   if (binFileName == NULL)
   {
      char linkName[1024];
      snprintf(linkName, sizeof(linkName), "/proc/%s/path/a.out", pid);
      int iLength = readlink(linkName, fileName, sizeof(fileName)-1);
      if (iLength <=0)
      {
         debug("Failed to resolve link %s",fileName);
         strncpy(fileName,linkName, sizeof(fileName));
      }
      else
      {
         fileName[iLength]='\0';
         debug("Resolved link %s to %s", linkName, fileName);
      }

      binFileName = fileName;
   }

   int elfID = openElfFile(p,  binFileName, 0, 0, 1);
   loadSymbols(p, elfID, 0);
   loadLibraries(p,elfID,"/", plddMode);

   getLWPsFromPID(p);

   return p;
}


#if defined (_LP64)
#define REG_IP 16
#define SIG_RETURN 0xffffffffffffffff
#else
#define REG_IP 14
#define SIG_RETURN 0xffffffff
#endif

Elf_Addr processSignalHandler(const mxProc * p, Elf_Addr stackLimit, Elf_Addr fp, Elf_Addr curr_ret_addr, int fullStack)
{

#if defined(__x86_64) || defined(__i386)
   if (curr_ret_addr == SIG_RETURN)
   {
      if (fullStack)
         printf("****** Signal handler\n");

      // ucontext_t is passed as the 3rd argument to the handler.  It contains the saved registers, allowing us to get the function pointer
      Elf_Addr ucontext_addr = 0;
#if defined (_LP64)
      // We can't read it as an arugument as it's passed via a register and lost.  However it appars to always be near the top of the frame.
      ucontext_addr = fp + 5 * sizeof(Elf_Addr);
#else
      readMxProcVM(p, fp + 4 * sizeof(fp), &ucontext_addr, sizeof(fp));
#endif
      ucontext_t ucontext;
      debug("reading ucontext from "FMT_ADR,ucontext_addr);
      readMxProcVM(p, ucontext_addr, &ucontext, sizeof(ucontext));

      return ucontext.uc_mcontext.gregs[REG_IP];
   }
#endif

   // We haven't detected a signal handler.  Return the passed in value
   return curr_ret_addr;

}

