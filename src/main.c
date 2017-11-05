/*
*  Copyright Murex S.A.S., 2003-2013. All Rights Reserved.
*
*  This software program is proprietary and confidential to Murex S.A.S and its affiliates ("Murex") and, without limiting the generality of the foregoing reservation of rights, shall not be accessed, used, reproduced or distributed without the
*  express prior written consent of Murex and subject to the applicable Murex licensing terms. Any modification or removal of this copyright notice is expressly prohibited.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <getopt.h>

//#include "lib/common/mxconst/h/mx_version.h"

#include "mxProcUtils.h"
#include "pmx.h"

int main(int argc, char *argv[])
{
   //These are the different modes we can run in
   Elf_Addr address = 0;
   int pargs = 0;
   int pldd = 0;
   int pmap = 0;
   int pstack = 0;
   int extract = 0;
   long dumpRawStack = 0;

   int opt;
   char * addressSymbol = NULL;
   char dataType[1024] = "RAW1K";
   int errflg = 0;
   int lwp = 1;
   long stackArguments = 8;
   char libraryRoot[1024] = "/";
   char filePrefix[1024] = "";
   char *command;
   int corruptStack=200;
   int force=0;

   if ((command = strrchr(argv[0], '/')) != NULL)
   {
      command++;
   }
   else
   {
      command = argv[0];
   }

   // If pmx has been (sym)linked to pargs, pldd, pmap or pstack, enable that mode by default
   if (!strcmp(command, "pargs"))
      pargs=1;
   if (!strcmp(command, "pldd"))
      pldd=1;
   if (!strcmp(command, "pstack"))
      pstack=1;
   if (!strcmp(command, "pmap"))
      pmap=1;

   // Our old version of Solaris has the name in struct option defined as char * rather than const char *
   // When we move to a newer version of Solaris, we can just use string literals directly in long_options
   char sz_args[]="args";
   char sz_pldd[]="pldd";
   char sz_pmap[]="pmap";
   char sz_type[]="type";
   char sz_all[]="all";
   char sz_extract[]="extract";
   char sz_all_threads[]="all-threads";
   char sz_help[]="help";
   char sz_inline[]="inline";
   char sz_corrupt[]="corrupt-stack";
   char sz_sysroot[]="sysroot";
   char sz_pargs[]="pargs";
   char sz_output_prefix[]="output-prefix";
   char sz_raw_stack[]="raw-stack";
   char sz_pstack[]="pstack";
   char sz_show_types[]="show-types";
   char sz_verbose[]="verbose";
   char sz_address[]="address";
   char sz_force[]="force";

   static struct option long_options[] = {
      {sz_args,         required_argument, 0, 'a' },
      {sz_pldd,         no_argument,       0, 'b' },
      {sz_pmap,         no_argument,       0, 'c' },
      {sz_type,         required_argument, 0, 'd' },
      {sz_all,          no_argument,       0, 'e' },
      {sz_all_threads,  no_argument,       0, 'f' },
      {sz_extract,      no_argument,       0, 'g' },
      {sz_help,         no_argument,       0, 'h' },
      {sz_inline,       no_argument,       0, 'i' },
      {sz_corrupt,      required_argument, 0, 'j' },
      {sz_force,        no_argument,       0, 'k' },
      {sz_sysroot,      required_argument, 0, 'l' },
      {sz_pargs,        no_argument,       0, 'm' },
      {sz_output_prefix,required_argument, 0, 'p' },
      {sz_raw_stack,    required_argument, 0, 'r' },
      {sz_pstack,       no_argument,       0, 's' },
      {sz_show_types,   no_argument,       0, 't' },
      {sz_verbose,      no_argument,       0, 'v' },
      {sz_address,      required_argument, 0, 'x' },
      {0,              0,                 0, 0   }
   };

   /* options */
   int opt_index=0;
   while ((opt = getopt_long(argc, argv, "a:bcd:efhij:l:mp:r:stvx:", long_options, &opt_index)) != -1)
   {
      switch (opt)
      {
         case 'a':
            stackArguments=strtol(optarg,NULL,10);
            break;
         case 'b':
            pldd = 1;
            break;
         case 'c':
            pmap = 1;
            break;
         case 'd':
            strncpy(dataType,optarg,sizeof(dataType));
            break;
         case 'e':
            pldd = 1; pargs = 1; pstack = 1; extract=1;
            break;
         case 'f':
            lwp = 0;
            break;
         case 'g':
            extract = 1;
            break;
         case 'h':
            errflg = 1;
            break;
         case 'i':
            setInlineMode(1);
            break;
         case 'j':
            corruptStack=strtol(optarg,NULL,10);
            break;
         case 'k':
            force = 1;
            break;
         case 'l':
            strncpy(libraryRoot,optarg,sizeof(libraryRoot));
            if (libraryRoot[strlen(libraryRoot)-1] != '/')
               strcat(libraryRoot,"/");
            break;
         case 'm':
            pargs = 1;
            break;
         case 'p':
            strncpy(filePrefix,optarg,sizeof(filePrefix));
            break;
         case 'r':
            dumpRawStack = strtol(optarg,NULL,0);
            break;
         case 's':
            pstack = 1; extract=1;
            break;
         case 't':
            printf("This pmx binary can print the following types:\n");
            display_type_printers();
            exit(0);
         case 'v':
            setVerbose(1);
            break;
         case 'x':
            if (strncmp("0x",optarg,2)==0)
               address = (Elf_Addr) strtoul(optarg, NULL, 16);
            else
               addressSymbol = optarg;
            break;
         default:
            errflg = 1;
            break;
      }
   }

   argc -= optind;
   argv += optind;


   if (errflg || argc < 1 || argc > 2)
   {
      //              "01234567890123456789012345678901234567890123456789012345678901234567890123456789\n");
      //fprintf(stderr, "pmx " mxVERSION_EXTERNAL_MAJOR " " mxBUILD_ID "\n");
      fprintf(stderr, "usage: %s [mode]... [option]... [binary] {pid|core}[/lwps]\n", command);
      fprintf(stderr, "\n");
      fprintf(stderr, "  binary: The name of the binary corresponding to the core/pid.\n");
      fprintf(stderr, "          This will be automatically detected if omitted.\n");
      fprintf(stderr, "  lwps:   The lwps/thread to analyse.  By default the first thread is analysed.\n");
      fprintf(stderr, "          To analyse all lwps/threads, use the --all-threads.\n");
      fprintf(stderr, "\n");
      fprintf(stderr, "Standard Modes:\n");
      fprintf(stderr, "  --extract, -g            Extract known data structures from stack. (default)\n");
      fprintf(stderr, "  --pargs, -m              Print process arguments.\n");
      fprintf(stderr, "  --pstack, -s             Print stack trace.  Implies --extract.\n");
      fprintf(stderr, "  --pldd, -b               Print loaded libraries.\n");
      fprintf(stderr, "  --all, -e                The same as --extract --pargs --pstack --pldd.\n");
      fprintf(stderr, "\n");
      fprintf(stderr, "Advanced Modes:\n");
      fprintf(stderr, "  --pmap, -c               Print memory map of process.\n");
      fprintf(stderr, "  --show-types, -t         Print supported data types.\n");
      fprintf(stderr, "  --raw-stack=n, -r n      Print n words from of the raw stack.\n");
      fprintf(stderr, "  --address=addr, -x addr  Print data structure at addr.  Use with --type.\n");
      fprintf(stderr, "                           addr can be a hex address (0x1234) or a symbol.\n");
      fprintf(stderr, "\n");
      fprintf(stderr, "Options:\n");
      fprintf(stderr, "  --inline, -i             Print all data to stdout rather than creating files.\n");
      fprintf(stderr, "  --force, -k              Run even if the binary doesn't match the core.\n");
      fprintf(stderr, "  --args=n, -a n           Print n (default 8) arguments for functions when we\n");
      fprintf(stderr, "                           don't know better.  Use with --pstack.\n");
      fprintf(stderr, "  --type=type, -d type     Type of data structure printed by --address.\n");
      fprintf(stderr, "                           Use -t to list supported types.  Default is RAW1K.\n");
      fprintf(stderr, "  --all-threads, -f        Process all threads, rather than just the first.\n");
      fprintf(stderr, "  --sysroot=path, -l path  Use path as system root for loading libraries with\n");
      fprintf(stderr, "                           absolute references. Like 'set sysroot' in gdb.\n");
      fprintf(stderr, "  --output-prefix=path, -p path\n");
      fprintf(stderr, "                           Use path as the prefix for output files.\n");
      fprintf(stderr, "                           If unspecified, the core file name is used.\n");
      fprintf(stderr, "  --corrupt-stack=n, -j n  Search this many words for a valid frame in the case\n");
      fprintf(stderr, "                           of stack corruption.  Default 200.\n");
      fprintf(stderr, "  --verbose, -v            Print pmx debugging/troubleshooing information.\n");
      exit(2);
   }

   // If no other modes are specified, default to extract mode
   if (!pldd && !pmap && !pargs && !address && !dumpRawStack )
      extract=1;

   const char *mx = NULL;
   if (argc==2)
   {
      mx = argv[0];
      argc--;
      argv++;
   }

   char *process = argv[0];

   // Open a process or a core file
   mxProc *p;
   char processBase[1024];

   // Strip off LWPID (only if it's numeric)
   strncpy(processBase, process, sizeof(processBase));
   for (int i=strlen(process)-1; i>=0; i--)
   {
      if (process[i] >= '0' &&  process[i] <= '9')
         continue;
      else if (process[i] == '/') {
         lwp = atoi(process+i+1);
         debug("Only displaying LWP %d", lwp);
         processBase[i]='\0';
         break;
      }
      else
         break;
   }

   // If the remainder is numeric, assume it's a PID
   int isCoreFile=0;
   for(unsigned int i=0; i<strlen(processBase); i++)
   {
      if (processBase[i] < '0' ||  processBase[i] > '9')
      {
         isCoreFile=1;
         break;
      }
   }

   struct rlimit rl;
   if (getrlimit(RLIMIT_NOFILE, &rl))
   {
      warning("Unable to detect current file limit");
   }
   else if (rl.rlim_cur < 1024)
   {
      rl.rlim_cur = 1024;
      if (rl.rlim_cur > rl.rlim_max)
         rl.rlim_cur = rl.rlim_max;

      if (setrlimit(RLIMIT_NOFILE, &rl))
      {
         warning("Unable to increase current file limit to %lu", rl.rlim_cur);
      }
   }

   // Open Process/Core.  This covers pldd functionality
   if (isCoreFile)
   {
      debug("opening core %s", processBase);
      p = openCoreFile(mx, processBase, libraryRoot, pldd);

#if defined(__linux)
      if (!p->pid && lwp==1)
      {
         // This happens on cores from valgrind
         warning("Unable to detect main thread ID.  All threads will be processed");
         lwp=0;
      }
#endif
   }
   else
   {
      debug("opening live process %s", processBase);
      p = openPID(mx, processBase, pldd);
   }

   // Check that pmx, binary and core/PID are consistent as it may lead to bad structures / symbols
   checkConsistency(p, force);

   // Override file prefix if specified
   if (filePrefix[0])
      strncpy(p->filePrefix,filePrefix,sizeof(p->filePrefix));

   // pmap functionality
   if (pmap)
      printpmap(p);

   // Print specific address/symbnol
   if (addressSymbol)
   {
      address = getSymbolAddress(p,addressSymbol);
      if (address)
         printf("Found Address " FMT_ADR " for Symbol [%s]\n",(unsigned long)address,addressSymbol);
      else
      {
         printf("Could not find address for symbol [%s]",addressSymbol);
         return 1;
      }

   }

   if (address)
   {
      print_arbitrary_type(p, address, dataType);
      printf("\n");
   }

   // pargs functionality
   int pargs_fallback=0;
   if (pargs)
   {
      // Use internal copy of argc/argv/arg_x which cover all arguments, not just items passed to main()
      // See lib/system/mdsystem/c/sysarg.c
      Elf_Addr addressArgc = getSymbolAddress(p,"_argc");
      Elf_Addr addressArgv = getSymbolAddress(p,"_argv");
      Elf_Addr addressArgx = getSymbolAddress(p,"arg_x");

      debug("Found arg addresses _argc:" FMT_ADR " _argv:" FMT_ADR " arg_x:" FMT_ADR,(unsigned long)addressArgc, (unsigned long)addressArgv, (unsigned long)addressArgx);
      if (print_mxargv(p, addressArgc, addressArgv, addressArgx))
         pargs_fallback=1;
   }

   if (pstack || pargs_fallback || extract || dumpRawStack )
   {
      for (int i = 0; i < p->nLWPs; i++)
      {
         if (!lwp                  // All threads
             || lwp == p->LWPs[i].lwpID    // Selected Thread
             || (lwp == 1 && p->pid == p->LWPs[i].lwpID))  // Special hack for linux to allow use of /1
         {
            if (!lwp)
               printf("**** LWP %d ****\n", p->LWPs[i].lwpID);

            if (dumpRawStack)
               dumpStack(p, p->LWPs[i], dumpRawStack);

            if (pstack || pargs_fallback || extract)
               printCallStack(p, p->LWPs[i], pstack, stackArguments, corruptStack);
         }
      }
   }

   // Close proc to free memory, file descriptors, etc
   closeMxProc(p);

   return (EXIT_SUCCESS);
}

