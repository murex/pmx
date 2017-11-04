/*
*  Copyright Murex S.A.S., 2003-2013. All Rights Reserved.
*
*  This software program is proprietary and confidential to Murex S.A.S and its affiliates ("Murex") and, without limiting the generality of the foregoing reservation of rights, shall not be accessed, used, reproduced or distributed without the
*  express prior written consent of Murex and subject to the applicable Murex licensing terms. Any modification or removal of this copyright notice is expressly prohibited.
*/
#include <stdio.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/procfs.h>
#include <errno.h>
#include <cxxabi.h>
#include <signal.h>
#include <ucontext.h>

#include "mxProcUtils.h"

void readFile(const mxProc * c, int elfID, Elf_Addr fileAddr, void *buffPointer, size_t size)
{
   if (lseek(c->elfFile[elfID].fd,fileAddr,SEEK_SET) != (off_t) fileAddr)
   {
      fatal_error("Failed to seek offset " FMT_ADR " in file %d.",fileAddr, elfID);
   }
   read(c->elfFile[elfID].fd, buffPointer, size);
}

void closeMxProcPID(mxProc * p)
{
   int i;

   for (i = 0; i < p->nLWPs; i++)
   {
      if (ptrace(PTRACE_DETACH, p->LWPs[i].lwpID, NULL, NULL) == -1)
      {
         perror("ptrace: ");
         printf("Failed to detatch from process %ld.\n", (long) p->LWPs[i].lwpID);
      }
   }
}

void getLWPsFromPID(mxProc * p)
{
   char fileName[128];

   snprintf(fileName, sizeof(fileName), "/proc/%d/task", p->pid);
   struct dirent **namelist;

   int nFiles = scandir(fileName, &namelist, 0, alphasort);
   int t;

   for (t = 0; t < nFiles; t++)
   {
      char *slwpID = namelist[t]->d_name;

      if (slwpID[0] == '.')     // . or ..
         continue;

      pid_t lwpID = atoi(slwpID);
      struct user_regs_struct regs;

      // We have already attached to the main process.  Also attach to the others
      if (p->pid != lwpID)
      {
         if (ptrace(PTRACE_ATTACH, lwpID, NULL, NULL) == -1)
         {
            perror("ptrace: ");
            fatal_error("Failed to attach to process/LWP %d", lwpID);
         }

         // We really should use waitpid(lwpID,&status,0) here, but there
         // appears to be some quirkly linux behaviour/bug that I can't work out
         usleep(1000);
      }

      if (ptrace(PTRACE_GETREGS, lwpID, NULL, &regs) == -1)
      {
         perror("ptrace: ");
         fatal_error("Unable to read LWP %d info", lwpID);
      }

      mxLWP_t *lwp = &(p->LWPs[p->nLWPs++]);

      lwp->lwpID = lwpID;
#if defined (__x86_64)
      lwp->fp = (Elf_Addr) regs.rbp;
      lwp->sp = (Elf_Addr) regs.rsp;
      lwp->ip = (Elf_Addr) regs.rip;
#else
      lwp->fp = (Elf_Addr) regs.ebp;
      lwp->sp = (Elf_Addr) regs.esp;
      lwp->ip = (Elf_Addr) regs.eip;
#endif
      lwp->stack = 0;
      lwp->stacksize = 0;

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

      unsigned int nOff = 0;

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

         //printf("Found note [Name Size: %d, Desc Size: %d, Desc Offset: %d, Total Data Size: %d Type: %d ]\n", n.n_namesz, n.n_descsz, descOffset, dataSize, n.n_type);
         if (n.n_type == NT_PRSTATUS)
         {
            struct elf_prstatus s;
            readFile(c, 0, fileAddr+descOffset, &s, sizeof(s));

            const struct user_regs_struct *regs = reinterpret_cast<const struct user_regs_struct *>(&(s.pr_reg));

            mxLWP_t *lwp = &(c->LWPs[c->nLWPs++]);

            lwp->lwpID = s.pr_pid;
            debug("Loaded LWPID %d",lwp->lwpID);

#if defined (__x86_64)
            lwp->fp = (Elf_Addr) regs->rbp;
            lwp->sp = (Elf_Addr) regs->rsp;
            lwp->ip = (Elf_Addr) regs->rip;
#else
            lwp->fp = (Elf_Addr) regs->ebp;
            lwp->sp = (Elf_Addr) regs->esp;
            lwp->ip = (Elf_Addr) regs->eip;
#endif
            lwp->stack = 0;
            lwp->stacksize = 0;
         }
         else if (n.n_type == NT_PRPSINFO)
         {
            struct elf_prpsinfo s;
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

int readMxProcVM(const mxProc * p, Elf_Addr vmAddr, void *buffPointer, size_t size)
{
   char *buff = static_cast<char *>(buffPointer);

   memset(buff, 0, size);       // Calling functions should check the return value, but in case they dont.....

   if (p->type == mxProcTypeCore)
   {
      // Find file address
      Elf_Addr fileAddr = 0;
      int elfFile = 0;
      getFileAddrFromCore(p, vmAddr, &fileAddr, &elfFile, COREFIRST);
      //debug("found file address " FMT_ADR " for " FMT_ADR " in %s",fileAddr,vmAddr,p->elfFile[elfFile].fileName);

      if (!fileAddr)
      {
         debug("Could not find vm address " FMT_ADR " in the core file.  Check pmap.", vmAddr);
         return 1;
      }

      Elf_Addr endAddr = 0;
      int endElfFile = 0;
      getFileAddrFromCore(p, vmAddr + size - 1, &endAddr, &endElfFile, COREFIRST);

      if (!endAddr)
      {
         debug("Memory address goes beyond mapped areas (base " FMT_ADR " + size %#lx).  Check pmap.", vmAddr, size);
         return 1;
      }

      if (elfFile!=endElfFile)
      {
         debug("Memory address " FMT_ADR " goes across 2 elf files.", vmAddr);
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
   }
   else if (p->type == mxProcTypePID)
   {
      long val;

      Elf_Off startOff = (unsigned long) vmAddr % (unsigned long) sizeof(long);   // TODO Reading off-alignment addresses and sizes needs further testing

      //debug("vmAddr " FMT_ADR " buff " FMT_ADR " size " FMT_ADR " startOff " FMT_ADR "\n",vmAddr, buff, size, startOff);

      if (startOff)
      {
         vmAddr -= (sizeof(long) - startOff);   // Roll back the vm address back a few bytes to be on a word boundary
         if ((val = ptrace(PTRACE_PEEKDATA, p->pid, vmAddr, NULL)) == -1 && errno)
         {
            //perror("ptrace: ");
            debug("Failed to read %ld bytes of data from " FMT_ADR " in PID %ld", sizeof(long), vmAddr, (long) p->pid);
            return 1;
         }
         //printf("vmAddr %x buff %x size %x startOff %x\n",vmAddr, buff, size, startOff);
         size_t nBytes = size >= startOff ? startOff : size;
         memcpy(buff, sizeof(long) - startOff + (char *) (&val), nBytes);
         vmAddr += sizeof(long);
         buff += startOff;
         if (size >= startOff) // Avoid overflow on small reads
            size -= startOff;
         else
            size = 0;
      }

      while (size)
      {
         if ((val = ptrace(PTRACE_PEEKDATA, p->pid, vmAddr, NULL)) == -1 && errno)
         {
            //perror("ptrace: ");
            debug("Failed to read %ld bytes of data from " FMT_ADR " in PID %ld", sizeof(long), vmAddr, (long) p->pid);
            return 1;
         }
         size_t nBytes = size < sizeof(long) ? size : sizeof(long);

         //printf("vmAddr %x buff %x size %x nBytes %x\n",vmAddr, buff, size, nBytes);
         memcpy(buff, &val, nBytes);
         vmAddr += nBytes;
         buff += nBytes;
         size -= nBytes;
      }
      //printf("vmAddr %x buff %x size %x \n",vmAddr, buff, size);
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

   abi::__cxa_demangle(symbolName, demangled, &tmp_size, &status ) ;

   if (status != 0 ) {
      debug("demangle of %s failed",symbolName);
      strcpy(demangled, symbolName);
      return;
   }
}

mxProc *openPID(const char *binFileName, const char *pid, int plddMode)
{
   mxProc *p = static_cast<mxProc *>(malloc(sizeof(mxProc)));

   initMxProc(p);
   p->type = mxProcTypePID;
   p->pid = atoi(pid);
   sprintf(p->filePrefix,"pmx.pid%s",pid);

   if (ptrace(PTRACE_ATTACH, p->pid, NULL, NULL) == -1)
   {
      perror("ptrace: ");
      fatal_error("Failed to attach to process %d", p->pid);
   }

   int status;

   if (p->pid != waitpid(p->pid, &status, 0) || !WIFSTOPPED(status))
      fatal_error("Process %d hasn't stopped", p->pid);

   char fileName[1024];
   if (binFileName == NULL)
   {
      char linkName[1024];
      snprintf(linkName, sizeof(fileName), "/proc/%s/exe", pid);
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


#if defined (_LP64) && (__x86_64)
// This is what the Linux signal handlers return to on 64it
static const unsigned char linux_sigreturn[] =
{
   0x48, 0xc7, 0xc0, 0x0f, 0x00, 0x00, 0x00, /* movq $0x0f, %rax */
   0x0f, 0x05                                /* syscall */
};

// Instruction pointer is reg 16 in pregs structures
#define REG_IP 16

#else
// This is what the Linux signal handlers return to on 32bit
static const unsigned char linux_sigreturn[] =
{
   0xb8, 0xad, 0x00, 0x00, 0x00 /* mov $0xad, %eax */
};

// Instruction pointer is reg 14 in pregs structures
#define REG_IP 14

#endif

Elf_Addr processSignalHandler(const mxProc * p, Elf_Addr stackLimit, Elf_Addr fp, Elf_Addr curr_ret_addr, int fullStack)
{
   // Read first few bytes of the return function to see if it's a signal handler return.
   // If so, we need to dig in to find the saved instruction pointer to get the actual
   // function being called when the signal was handled.
   unsigned char inst[sizeof(linux_sigreturn)];
   readMxProcVM(p, curr_ret_addr, inst, sizeof(inst));
   if (memcmp(inst, linux_sigreturn, sizeof(inst)) == 0)
   {
      // siginfo_t is the 2nd argument to the handler.  It contains some interesting information.
#if defined (_LP64)
      // We haven't worked out how to get siginfo on x64 as it's passed via a register that is lost
      if (fullStack)
         printf("****** Signal handler\n");
#else
      Elf_Addr siginfo_addr = 0;
      readMxProcVM(p, fp + 3 * sizeof(fp), &siginfo_addr, sizeof(fp));
      siginfo_t siginfo;
      debug("reading siginfo from "FMT_ADR,siginfo_addr);
      readMxProcVM(p, siginfo_addr, &siginfo, sizeof(siginfo));

      if (fullStack)
         printf("****** Signal handler signo: %d, errno: %d, code: %d, addr: " FMT_ADR "\n", siginfo.si_signo, siginfo.si_errno, siginfo.si_code, (unsigned long)siginfo.si_addr);
#endif


      // ucontext_t is passed as the 3rd argument to the handler.  It contains the saved registers, allowing us to get the function pointer
      Elf_Addr ucontext_addr = 0;
#if defined (_LP64)
      // We can't read it as an arugument as it's passed via a register and lost.  However it appars to always be at the top of the frame.
      ucontext_addr = fp + 2 * sizeof(Elf_Addr);
#else
      readMxProcVM(p, fp + 4 * sizeof(fp), &ucontext_addr, sizeof(fp));
#endif
      ucontext_t ucontext;
      debug("reading ucontext from " FMT_ADR, ucontext_addr);
      readMxProcVM(p, ucontext_addr, &ucontext, sizeof(ucontext));

      return ucontext.uc_mcontext.gregs[REG_IP];
   }
   else
   {
      // We haven't detected a signal handler.  Return the passed in value
      return curr_ret_addr;
   }
}

