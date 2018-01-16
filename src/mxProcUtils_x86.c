/*******************************************************************************
*
* Copyright (c) {2003-2018} Murex S.A.S. and its affiliates.
* All rights reserved. This program and the accompanying materials
* are made available under the terms of the Eclipse Public License v1.0
* which accompanies this distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*
*******************************************************************************/

/*
*  Copyright Murex S.A.S., 2003-2013. All Rights Reserved.
*
*  This software program is proprietary and confidential to Murex S.A.S and its affiliates ("Murex") and, without limiting the generality of the foregoing reservation of rights, shall not be accessed, used, reproduced or distributed without the
*  express prior written consent of Murex and subject to the applicable Murex licensing terms. Any modification or removal of this copyright notice is expressly prohibited.
*/
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>

#include "mxProcUtils.h"

static Elf_Addr getNextFrame(const mxProc * p, Elf_Addr stackLimit, Elf_Addr fp)
{
   Elf_Addr next_frame = 0;
   int fileID = 0;

   getFileAddrFromCore(p, fp, &next_frame, &fileID, COREONLY); // We only search for stack frames in the core

   if (fileID)
   {
      debug("Next frame goes outside core file (not on stack) (" FMT_ADR ")", (unsigned long) next_frame);
      return 0;
   }

   if (p->type == mxProcTypeCore && ! next_frame)
   {
      debug("Unable to read next frame from (" FMT_ADR ")", (unsigned long) fp);
      return 0;
   }
   // TODO - impliment non-fatal way to test if address is outside VM for live processes

   readMxProcVM(p, fp, &next_frame, sizeof(fp));

   if (!next_frame)
      return 0;

   if (next_frame >= stackLimit)
   {
      debug("Next frame goes beyond stack (" FMT_ADR " >= " FMT_ADR ")", (unsigned long) next_frame, (unsigned long) stackLimit);
      return 0;
   }

   if (next_frame <= fp)
   {
      debug("Next frame after current frame (" FMT_ADR " <= " FMT_ADR ")", (unsigned long) next_frame, (unsigned long) fp);
      return 0;
   }

   if ((long) next_frame & (sizeof(long) - 1))
   {
      debug("Next frame not on word boundary " FMT_ADR "", (unsigned long) next_frame);
      return 0;
   }

   if (next_frame - fp > 0x10000)
   {
      debug("Next frame far from current frame (" FMT_ADR " - " FMT_ADR " > 0x10000)", (unsigned long) next_frame, (unsigned long) fp);
      // return 0; // Lets not fail, as mx has been known to have some massive frames
   }

   debug("Next frame is ok (" FMT_ADR ")", (unsigned long) next_frame);
   return next_frame;
}

static int verifyFramePointer(const mxProc * p, Elf_Addr stackLimit, Elf_Addr fp)
{
   // Verify that this looks like a valid frame pointer
   // If we have at least 4 valid frames, then we decide it's good.

   int i;

   for (i = 0; i < 3; i++)
   {
      fp = getNextFrame(p, stackLimit, fp);
      if (!fp)
         return i;
   }

   return i;
}

static void recurseCallStack(const mxProc * p, Elf_Addr stackLimit, Elf_Addr fp, int fullStack, int stackArguments, int corruptStackSearch)
{
   for (int iFrames=1; iFrames++ ; )
   {
      debug("Printing frame at " FMT_ADR, fp);
      Elf_Addr curr_ret_addr = 0;

      readMxProcVM(p, fp + sizeof(fp), &curr_ret_addr, sizeof(fp));

      //Adjust in case we are in a signal handler
      curr_ret_addr = processSignalHandler(p, stackLimit, fp, curr_ret_addr, fullStack);

      Elf_Addr nextfp = getNextFrame(p, stackLimit, fp);

      if (!nextfp)
      {
         // In case this is a corrupt stack, lets look forward a bit to see if we can see a valid stack frame and resume there
         nextfp=fp;
         int i;
         for (i = corruptStackSearch; i > 0; i--)
         {
            if (verifyFramePointer(p, stackLimit, nextfp) >= 3)
               break;
            nextfp = nextfp + sizeof(Elf_Addr);
         }

         if (!i)
            return;

         warning("Stack corruption detected.  Some stack frames will be missing!");

      }

      fp=nextfp;

      printStackItem(p, curr_ret_addr, fp, fullStack, stackArguments);

      if (iFrames == 300)
         warning("Stack overflow detected. pmx may take a long time to complete.");
      else if (!(iFrames % 1000))
         warning("Processed %d frames, currently at " FMT_ADR, iFrames, fp);
   }
}

void printCallStack(const mxProc * p, mxLWP_t t, int fullStack, int stackArguments, int corruptStackSearch)
{

   // On linux, we don't have stack info, so just set the limit to the top of the memory and hope for the best
   Elf_Addr stackLimit = t.stack ? t.stack + t.stacksize : (Elf_Addr) ULONG_MAX;

   // t.fp should point to the top of the next frame on the stack, but in some weird situations on linux it doesn't
   // As an evil work around, lets just look through the first few items on the stack to see if we stumble
   // upon what looks like a valid frame pointer.  Ideally we would work out a proper way to unroll the frame
   // First pass we want to see subsequent valid frames.  If that fails, we will settle for 1.

   //First try the frame pointer, as that is its purpose
   Elf_Addr fp = t.fp;
   if (verifyFramePointer(p, stackLimit, fp) < 3)
   {
      // Next look down the stack from stack pointer
      fp = t.sp;
      int i;
      for (i = corruptStackSearch; i > 0; i--)
      {
         debug("Testing frame " FMT_ADR " for 3",fp);
         if (verifyFramePointer(p, stackLimit, fp) >= 3)
            break;
         fp = fp + sizeof(Elf_Addr);
      }

      // Next look up the stack from stack pointer
      if (!i)
      {
         fp = t.sp;
         for (i = corruptStackSearch; i > 0; i--)
         {
            debug("Testing frame " FMT_ADR " for 3",fp);
            if (verifyFramePointer(p, stackLimit, fp) >= 3)
               break;
            fp = fp - sizeof(Elf_Addr);
         }
      }

      // Next look down the stack from stack pointer with a lower threshold
      if (!i)
      {
         fp = t.sp;
         for (i = corruptStackSearch; i > 0; i--)
         {
            debug("Testing frame " FMT_ADR " for 1",fp);
            if (verifyFramePointer(p, stackLimit, fp) >= 1)
               break;
            fp = fp + sizeof(Elf_Addr);
         }
      }

      // Give up
      if (!i)
      {
         debug("Couldn't find a good frame");
         return;
      }

      debug("Starting stack trace at " FMT_ADR " (fp " FMT_ADR " + %lx)", (unsigned long) fp, (unsigned long) t.fp, (unsigned long) fp - (unsigned long) t.fp);
   }

   printStackItem(p, t.ip, fp, fullStack, stackArguments);
   recurseCallStack(p, stackLimit, fp, fullStack, stackArguments, corruptStackSearch);
}


int getFloatArgNumber(unsigned char rex, unsigned char modrm, int verbose)
{
   //Register is encoded in bits 3->5.
   modrm &= 0x38;

   if (modrm==0x00)
   {
      verbose && printf("%%xmm0");
      return 0;
   }
   else if (modrm==0x08)
   {
      verbose && printf("%%xmm1");
      return 1;
   }
   else if (modrm==0x10)
   {
      verbose && printf("%%xmm2");
      return 2;
   }
   else if (modrm==0x18)
   {
      verbose && printf("%%xmm3");
      return 3;
   }
   else if (modrm==0x20)
   {
      verbose && printf("%%xmm4");
      return -1;
   }
   else if (modrm==0x28)
   {
      verbose && printf("%%xmm5");
      return -1;
   }
   else if (modrm==0x30)
   {
      verbose && printf("%%xmm6");
      return -1;
   }
   else if (modrm==0x38)
   {
      verbose && printf("%%xmm7");
      return -1;
   }

   verbose && printf("%%error");
   return -1;
}

int getArgNumber(unsigned char rex, unsigned char modrm, int verbose)
{
   //We are assuming the register is encoded in bits 3->5.  The most significant extension bit comes from rex.
   modrm &= 0x38;

   //Decodes the two characters passed to a movq/movl/movb command to determine the register
   if (! (rex & 0x4))
   {
      if (modrm==0x00)
      {
         verbose && printf("%%rax");
         return -1;
      }
      else if (modrm==0x08)
      {
         verbose && printf("%%rcx");
         return 3;
      }
      else if (modrm==0x10)
      {
         verbose && printf("%%rdx");
         return 2;
      }
      else if (modrm==0x18)
      {
         verbose && printf("%%rbx");
         return -1;
      }
      else if (modrm==0x20)
      {
         verbose && printf("%%rsp");
         return -1;
      }
      else if (modrm==0x28)
      {
         verbose && printf("%%rbp");
         return -1;
      }
      else if (modrm==0x30)
      {
         verbose && printf("%%rsi");
         return 1;
      }
      else if (modrm==0x38)
      {
         verbose && printf("%%rdi");
         return 0;
      }
   }
   else
   {
      if (modrm==0x00)
      {
         verbose && printf("%%r8");
         return 4;
      }
      else if (modrm==0x08)
      {
         verbose && printf("%%r9");
         return 5;
      }
      else if (modrm==0x10)
      {
         verbose && printf("%%r10");
         return -1;
      }
      else if (modrm==0x18)
      {
         verbose && printf("%%r11");
         return -1;
      }
      else if (modrm==0x20)
      {
         verbose && printf("%%r12");
         return -1;
      }
      else if (modrm==0x28)
      {
         verbose && printf("%%r13");
         return -1;
      }
      else if (modrm==0x30)
      {
         verbose && printf("%%r14");
         return -1;
      }
      else if (modrm==0x38)
      {
         verbose && printf("%%r15");
         return -1;
      }
   }

   verbose && printf("%%error");
   return -1;
}


static void getArguments64(const mxProc *proc, Elf_Addr disAddr, Elf_Addr rbp, int verbose, mxArguments *args)
{
   // Very basic disassembly of start of function to get saved arguments passed via
   // registers and saved on stack.  Only handles unoptimised code and likely
   // needs revisting if we change compilers or compiler versions as this behaviour is
   // not defined by any spec
   //
   // Will only work if passed an address at the start of a function

   // Some useful info at:
   // http://ref.x86asm.net/coder64.html
   // http://wiki.osdev.org/X86-64_Instruction_Encoding

   unsigned char rawdiss[8];
   if (readMxProcVM(proc,disAddr,&rawdiss,sizeof(rawdiss)))
      return;

   // Frame pointer save
   if (rawdiss[0] == 0x55) // pushq %rsp
   {
      verbose && printf(FMT_ADR ": pushq    %%rbp\n", (unsigned long)disAddr);
      disAddr++;
   }
   else
   {
      debug("Expected Frame Pointer Save - found %x", rawdiss[0]);
      return;
   }

   // Set new frame base - TODO - make this more generic
   if ((rawdiss[1]==0x48 && rawdiss[2]==0x89 && rawdiss[3]==0xe5) || // GCC
       (rawdiss[1]==0x48 && rawdiss[2]==0x8b && rawdiss[3]==0xec)) // Solaris Studio
   {
      verbose && printf(FMT_ADR ": movq     %%rsp,%%rbp\n", (unsigned long)disAddr);
      disAddr+=3;
   }
   else
   {
      debug("Expected new frame base to be set");
      return;
   }

   // Soak up any 32bit or 64bit register saves we don't care for
   // gcc does this sometimes.  From what we see, this is only for non-argument registers
   // so we can just ignore them
   while (true)
   {
      if (readMxProcVM(proc,disAddr,&rawdiss,sizeof(rawdiss)))
         return;

      if (rawdiss[0]==0x41 && rawdiss[1]>=0x50 && rawdiss[1]<0x58) // pushq 64bit
      {
         verbose && printf(FMT_ADR ": pushq    reg %d\n", (unsigned long)disAddr, rawdiss[1]-0x50);
         disAddr+=2;
      }
      else if (rawdiss[0]>=0x50 && rawdiss[0]<0x58) // push 32bit
      {
         verbose && printf(FMT_ADR ": push     reg %d\n", (unsigned long)disAddr, rawdiss[0]-0x50);
         disAddr+=1;
      }
      else
      {
         break;
      }
   }

   //Look for Stack Pointer Move
   if (readMxProcVM(proc,disAddr,&rawdiss,sizeof(rawdiss)))
      return;

   if (rawdiss[0]==0x48 && rawdiss[1]==0x81 && rawdiss[2]==0xec) // move stack point 32bit
   {
      // have to do nasty stuff to make gcc expand the size
      unsigned int *n = reinterpret_cast<unsigned int *>(reinterpret_cast<void*>(rawdiss+3));
      verbose && printf(FMT_ADR ": subq     0x%x,%%rsp\n", (unsigned long)disAddr, *n);
      disAddr+=7;
   }
   else if (rawdiss[0]==0x48 && rawdiss[1]==0x83 && rawdiss[2]==0xec) // move stack point 8bit
   {
      unsigned char *n = rawdiss+3;
      verbose && printf(FMT_ADR ": subq     0x%hhx,%%rsp\n", (unsigned long)disAddr, *n);
      disAddr+=4;
   }
   else
   {
      debug("Didn't find stack pointer move. Continuing anyway.");
   }

   // Look for any moves of registers to the stack, and if it's an argument register, save it
   while (true)
   {
      if (readMxProcVM(proc,disAddr,&rawdiss,sizeof(rawdiss)))
         return;

      unsigned char instruction=0;
      unsigned char rex=0;
      unsigned char modrm=0;
      signed char *addroff1=0;
      signed int *addroff4=0;
      int addrLength=0;

      verbose && printf(FMT_ADR ": ", (unsigned long)disAddr);
      if (rawdiss[0] == 0xf2 && rawdiss[1] == 0x0f && rawdiss[2] == 0x11)  // movsd
      {
         instruction=rawdiss[2];
         modrm=rawdiss[3];
         disAddr+=4;
         addroff1=reinterpret_cast<signed char *>(rawdiss+4);
         addroff4=reinterpret_cast<signed int *>(rawdiss+4);
      }
      else if ((rawdiss[0] & 0xf0) == 0x40) // REX extension byte
      {
         rex=rawdiss[0];
         instruction=rawdiss[1];
         modrm=rawdiss[2];
         disAddr+=3;
         addroff1=reinterpret_cast<signed char *>(rawdiss+3);
         addroff4=reinterpret_cast<signed int *>(rawdiss+3);
      }
      else
      {
         rex=0x0;
         instruction=rawdiss[0];
         modrm=rawdiss[1];
         disAddr+=2;
         addroff1=reinterpret_cast<signed char *>(rawdiss+2);
         addroff4=reinterpret_cast<signed int *>(rawdiss+2);
      }

      // first two bits of modrm tell us if it's a 8 bit or 32bit offset
      Elf_Addr argAddr = 0x0;
      if ((modrm & 0xc0) == 0x40)
      {
         argAddr = rbp + *addroff1;
         addrLength = 1;
      }
      else if ((modrm & 0xc0) == 0x80)
      {
         argAddr = rbp + *addroff4;
         addrLength = 4;
      }
      else
      {
         debug("print register offset");
         return;
      }
      disAddr += addrLength;


      if (instruction==0x89 && (rex & 0x48) == 0x48) // movq 64bit reg
      {
         verbose && printf("movq     ");
         int argNo = getArgNumber(rex,modrm,verbose);
         if (argNo >= 0)
            addIntArgument(proc, args, argNo, argAddr, 8);
      }
      else if (instruction==0x89) // movl 32bit reg
      {
         verbose && printf("movl     ");
         int argNo = getArgNumber(rex,modrm,verbose);
         if (argNo >= 0)
            addIntArgument(proc, args, argNo, argAddr, 4);
      }
      else if (instruction==0x88) // movb 8bit Reg
      {
         verbose && printf("movb     ");
         int argNo = getArgNumber(rex,modrm,verbose);
         if (argNo >= 0)
            addIntArgument(proc, args, argNo, argAddr, 1);
      }
      else if (instruction==0x11) // movsd - double
      {
         verbose && printf("movsd    ");
         int argNo = getFloatArgNumber(rex,modrm,verbose);
         if (argNo >= 0)
            addFloatArgument(proc, args, argNo, argAddr, 8);
      }
      else
         break;


      if (addrLength==1)
         verbose && printf(",0x%hhx(%%rbp)\n", *addroff1);
      else
         verbose && printf(",0x%x(%%rbp)\n", *addroff4);
   }

   verbose && printf("finished decompiling\n");
   return;
}

mxArguments *getArguments(const mxProc *proc, Elf_Addr disAddr, Elf_Addr frameAddr, int verbose)
{
   mxArguments *args = reinterpret_cast<mxArguments *>(calloc(sizeof(mxArguments),1));

#if defined (_LP64)
   getArguments64(proc,disAddr,frameAddr,verbose, args);
#endif

   for (int i=0 ; i<MAX_ARGS; i++)
      addStackArgument(proc, args, i, frameAddr + (2+i)*sizeof(long), sizeof(long));

   return args;
}
