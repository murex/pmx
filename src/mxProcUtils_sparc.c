/*
*  Copyright Murex S.A.S., 2003-2013. All Rights Reserved.
*
*  This software program is proprietary and confidential to Murex S.A.S and its affiliates ("Murex") and, without limiting the generality of the foregoing reservation of rights, shall not be accessed, used, reproduced or distributed without the
*  express prior written consent of Murex and subject to the applicable Murex licensing terms. Any modification or removal of this copyright notice is expressly prohibited.
*/
#include <stdio.h>
#include <stdlib.h>

#include "mxProcUtils.h"

#if defined (_LP64)
Elf_Addr bias = 0x7ff;
#else
Elf_Addr bias = 0x0;
#endif

static Elf_Addr getNextFrame(const mxProc * p, Elf_Addr stackLimit, Elf_Addr fp)
{
   Elf_Addr next_frame = 0;

   readMxProcVM(p, fp + 14 * sizeof(fp), &next_frame, sizeof(fp));

   next_frame+=bias;

   if (!next_frame)
      return 0;

   if (next_frame >= stackLimit)
   {
      debug("Next frame goes beyond stack ("FMT_ADR" >= "FMT_ADR")", (unsigned long) next_frame, (unsigned long) stackLimit);
      return 0;
   }

   if (next_frame <= fp)
   {
      debug("Next frame after current frame ("FMT_ADR" <= "FMT_ADR")", (unsigned long) next_frame, (unsigned long) fp);
      return 0;
   }

   if ((long) next_frame & (sizeof(long) - 1))
   {
      debug("Next frame not on word boundary "FMT_ADR"", (unsigned long) next_frame);
      return 0;
   }

   if (next_frame - fp > 0x10000)
   {
      debug("Next frame too far from current frame ("FMT_ADR" - "FMT_ADR" > 0x10000)", (unsigned long) next_frame, (unsigned long) fp);
      // return 0; // Lets not fail, as mx has been known to have some massive frames
   }

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
        debug("Printing frame at "FMT_ADR,fp);
        Elf_Addr curr_ret_addr = 0;

        readMxProcVM(p, fp + 15 * sizeof(fp), &curr_ret_addr, sizeof(fp));

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
           warning("Processed %d frames, currently at "FMT_ADR, iFrames, fp);
    }
}

void printCallStack(const mxProc * p, mxLWP_t t, int fullStack, int stackArguments, int corruptStackSearch)
{
   printStackItem(p, t.ip, t.sp+bias, fullStack, stackArguments);

   Elf_Addr stackLimit = t.stack + t.stacksize;

   recurseCallStack(p, stackLimit, t.sp+bias, fullStack, stackArguments, corruptStackSearch);
}

mxArguments *getArguments(const mxProc *proc, Elf_Addr disAddr, Elf_Addr frameAddr, int verbose)
{
    mxArguments *args = reinterpret_cast<mxArguments *>(calloc(sizeof(mxArguments),1));
   
    for (int i=0 ; i<MAX_ARGS; i++)
        addStackArgument(proc, args, i, frameAddr + (8+i)*sizeof(long), sizeof(long));

    return args;
}

