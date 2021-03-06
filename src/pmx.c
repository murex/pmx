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
#include <set>
#include <string.h>

#include "mxProcUtils.h"
#include "pmx.h"

static int inlineMode=0;

static int printArgv=0; // enabler flag for  print_main_argv

// Note that almost all structures are passed as pointes.  Make sure you include * in the definition below.
static TypePrinterEntry *type_printer_for_type = NULL;
static TypePrinterEntry type_printer_default[] = {
   // Generic types and tools.  Many are turned off by default to avoid noise
   {"char", print_string_argument, "Null terminated array", 1, 1},
   {"std::string", print_std_string, NULL, 1, 1},
   {"double", print_double_argument, "Use 'double' to cast supplied value as a double", 0, 0},
   {"double", print_double_pointer, "Use 'double*' to read double from memory", 1, 0},
   {"int", print_int_argument, "Use 'int' to cast supplied value as an int", 0, 0},
   {"int", print_int_pointer, "Use 'int*' to read int from memory", 1, 0},
   {"pointer", print_void_pointer, "Use to dereference a pointer locations", 1, 0},
   {"RAW1K", print_raw1k, "Dumps 1kB of raw data", 1, 0},
//    {"DISASSEMBLE", print_disassemble, NULL, 1, 0}, // Only used for development
   { NULL, NULL , NULL, 1, 0}
};

static FunctionPrinterEntry function_printers[] = {
   {"main", print_main_argv},

   {"jni_SetByteArrayRegion", print_jvm_full},
   {"mxSADocumentAdd", print_jvm_full},

   {"strlen", print_strlen},
   {"strcpy", print_strcpy},
   {"strncpy", print_strcpy},
   {"sprintf", print_sprintf},
   {"snprintf", print_snprintf},
   {"strcmp", print_strcmp},
   {"strncmp", print_strcmp},

   {"malloc", print_corrupt_heap},
   {"calloc", print_corrupt_heap},
   {"realloc", print_corrupt_heap},
   {"free", print_corrupt_heap},
#if defined(__linux)
   {"_int_malloc", print_corrupt_heap},
   {"__libc_calloc", print_corrupt_heap},
   {"malloc_consolidate", print_corrupt_heap},
   {"cfree", print_corrupt_heap},
   {"_int_free", print_corrupt_heap},
#elif defined(__sun)
   {"realfree", print_corrupt_heap},
#endif

#if defined(__linux)
   {"__libc_message", print_libc_message},
#endif

   { NULL, NULL}
};

int append_type_printers(TypePrinterEntry *ext)
{
   debug("Appending type printers");
   
   int size_orig = 0;
   int size_ext = 0;
   TypePrinterEntry *p = ext;
   while( p && p->type_name )
   {
      size_ext++;
      p++;
   }

   p = type_printer_default;
   while( p && p->type_name )
   {
      size_orig++;
      p++;
   }

   debug("type_printer_default size = %d, ext size = %d", size_orig, size_ext);
   type_printer_for_type = (TypePrinterEntry *)calloc(sizeof(TypePrinterEntry), size_orig + size_ext + 1);   

   memcpy(type_printer_for_type, type_printer_default, sizeof(TypePrinterEntry) * size_orig);
   memcpy(type_printer_for_type + size_orig, ext, sizeof(TypePrinterEntry) * size_ext);

   return size_orig + size_ext;
}

static StackHandler *lookup_print_function(const char *type_name, bool auto_detect)
{
   TypePrinterEntry *pEntry = type_printer_for_type;
   StackHandler *pReturn = NULL;

   int typeLen=strlen(type_name);

   int pointer = type_name[typeLen-1]=='*' || type_name[typeLen-1]=='&';

   char *type_name_short = strdup(type_name); // Copy without the pointer symbol (& or *)
   if (pointer)
      type_name_short[typeLen-1] = '\0';

   while (pEntry->type_name)
   {
      if (pointer == pEntry->pointer && strcmp(type_name_short, pEntry->type_name) == 0 && (!auto_detect || pEntry->auto_detect))
      {
         pReturn=pEntry->stack_handler;
         break;
      }
      pEntry++;
   }
   free (type_name_short);
   return pReturn;
}

void print_arbitrary_type(const mxProc *proc, Elf_Addr addr, const char *dataType)
{
   StackHandler *handler = lookup_print_function(dataType, false);

   //If we don't find a handler, interpret as a pointer and try again
   if (!handler)
   {
      char dataTypePointer[1024];
      snprintf(dataTypePointer,sizeof(dataTypePointer),"%s*",dataType);
      handler = lookup_print_function(dataTypePointer, false);
   }

   if (!handler)
   {
      warning("Unable to find handler for type %s.  Use -t to list supported types.", dataType);
   }
   else
   {
      char saddr[32];
      snprintf(saddr, sizeof(saddr), FMT_ADR, (unsigned long) addr);
      handler(proc,saddr,dataType,addr);
   }
}

void print_double(const char *function_name, const char *type, const mxProc * proc, Elf_Addr offset)
{
   if( type[0] == '*' )
   {
      Elf_Addr a = read_addr(proc, offset);
      double v = read_double(proc, a);
      printf("%s: %s=%f (at 0x%x)\n", function_name, type, v, a);
   }
   else
   {
      double v = read_double(proc, offset);
      printf("%s: %s=%f\n", function_name, type, v);
   }
}

void print_long(const char *function_name, const char *type, const mxProc * proc, Elf_Addr offset)
{
   if( type[0] == '*' )
   {
      Elf_Addr a = read_addr(proc, offset);
      long v = read_long(proc, a);
      printf("%s: %s=%ld (at 0x%x)\n", function_name, type, v, a);
   }
   else
   {
      long v = read_long(proc, offset);
      printf("%s: %s=%ld\n", function_name, type, v);
   }
}

void print_int(const char *function_name, const char *type, const mxProc * proc, Elf_Addr offset)
{
   if( type[0] == '*' )
   {
      Elf_Addr a = read_addr(proc, offset);
      int v = read_int(proc, a);
      printf("%s: %s=%d (at 0x%x)\n", function_name, type, v, a);
   }
   else
   {
      int v = read_int(proc, offset);
      printf("%s: %s=%d\n", function_name, type, v);
   }
}

void print_double_with_decimal(const char *function_name, const char *type, mxProc * proc, Elf_Addr offset)
{
   double v = read_double(proc, offset);

   printf("%s: %s=%f\n", function_name, type, v);
}

void print_string(const char *function_name, const char *type, const mxProc * proc, Elf_Addr offset, int skipIfEmpty, int maxLength)
{
#if defined(_LP64)
   int maxString = 1024 * 1024 * 1024; // 1G
#else
   int maxString = 1024 * 1024 * 100; // 100M
#endif

   if (!offset)
   {
      // For now, skipIfEmpty only applies to empty strings, not NULL pointers
      printf("%s: %s=NULL\n", function_name, type);
      return;
   }

   char *v = (char *)malloc(maxString);

   size_t strLength = 0;

   read_string(proc, offset, v, (maxLength ? maxLength : maxString));
   strLength = strlen(v);

   if (strLength == maxString-1)
      warning("String truncated to %dMB", maxString/(1024*1024));

   if (getInlineMode()==0 && (strLength > 1024 || strchr(v,'\n')))
   {
      char fileName[1024];
      static int stringCounter = 0;
      snprintf(fileName,sizeof(fileName),"%s.%d.txt",proc->filePrefix,stringCounter++);
#if defined(__sun) && !defined (_LP64)
      FILE *fp = fopen(fileName,"wF");
#else
      FILE *fp = fopen(fileName,"w");
#endif
      if (!fp)
      {
         warning("Failed to open %s for output. Maybe you need to add -p to the command line.",fileName);
         free(v);
         return;
      }

      printf("%s: %s written to %s (%ld bytes)\n", function_name, type, fileName, (long) strLength);
      fwrite(v, sizeof(char), strlen(v), fp);  // We don't use fprintf as it upsets the security scanner
      fclose(fp);
   }
   else if(!skipIfEmpty || v[0])
   {
      printf("%s: %s=\"%s\"\n", function_name, type, v);
   }
   free(v);
}

void print_string(const char *function_name, const char *type, const mxProc * proc, Elf_Addr offset, int skipIfEmpty)
{
   print_string(function_name,type,proc,offset,skipIfEmpty,0);
}

void print_string(const char *function_name, const char *type, const mxProc * proc, Elf_Addr offset)
{
   print_string(function_name,type,proc,offset,0,0);
}

void print_string_pointer(const char *function_name, const char *type, const mxProc * proc, Elf_Addr offset)
{
   Elf_Addr ptr = read_addr(proc, offset);
   print_string(function_name, type, proc, ptr);
}

void print_null(const mxProc * proc, const char *name, const char *comment)
{
   printf("%s: %s=NULL\n", name, comment);
}

void print_double_pointer(const mxProc * proc, const char *name, const char *comment, Elf_Addr value)
{
   print_double(name, comment, proc, value);
}

void print_double_argument(const mxProc * proc, const char *name, const char *comment, Elf_Addr value)
{
   // This is to print a string that's passed in by value, not reference
   printf("%s: %s=%f\n",name, comment, static_cast<double>(value));
}

void print_int_pointer(const mxProc * proc, const char *name, const char *comment, Elf_Addr value)
{
   print_int(name, comment, proc, value);
}

void print_int_argument(const mxProc * proc, const char *name, const char *comment, Elf_Addr value)
{
   // This is to print a string that's passed in by value, not reference
   printf("%s: %s=%d\n",name, comment, (int)value);
}

void print_void_pointer(const mxProc * proc, const char *name, const char *comment, Elf_Addr value)
{
   Elf_Addr addr = read_addr(proc, value);
   printf("%s: %s=" FMT_ADR "\n", name, comment, (unsigned long)addr);
}

void print_string_argument(const mxProc * proc, const char *name, const char *comment, Elf_Addr value)
{
   print_string(name, comment, proc, value);
}

void print_std_string(const mxProc * proc, const char *name, const char *comment, Elf_Addr value)
{
   // On supported platforms, we can simply dereference std::strings to find the char*
   print_string_pointer(name, comment, proc, value);
}

static FunctionHandler *lookup_function_handler(const char *function_name)
{
   FunctionPrinterEntry *pEntry = function_printers;

   while (pEntry->function_name)
   {
      if (strcmp(function_name, pEntry->function_name) == 0)
         return pEntry->function_handler;

      pEntry++;
   }
   return NULL;
}

void display_arguments(const mxProc * proc, const char *name, mxArguments *args)
{
   FunctionHandler *fentry = lookup_function_handler(name);
   if (fentry)
   {
      debug("found function handler for %s", name);
      if (fentry(proc, name, "", args))
      {
         debug("instructed by function handler not process function arguments");
         return;
      }
   }

   debug("looking for printer functions for function %s", name);

   static std::set<Elf_Addr> processed; // Keep a set of the addresses already displayed

   StackHandler *entry;
   char tmp_label[100];
   const char *type_name = args->arg[0].type;
   int i = 0;
   while (type_name != NULL && i < (args->count))
   {
      Elf_Addr val = args->arg[i].val.val;

      if (args->arg[i].size == 0)
         debug("Skipping uninitialised argument %d", i);
      else if (processed.find(val) != processed.end())
         debug("Skipping argument as we have already processed " FMT_ADR, val);
      else
      {
         debug("looking for printer function for type %s", type_name);
         entry = lookup_print_function(type_name, true);
         if (entry)
         {
            sprintf(tmp_label, "arg %d", i+1); // Convention is to start counting at 1
            debug("found print function for %s %d %s", name, i, type_name);
            if (val)
            {
               entry(proc, name, tmp_label, val);
               processed.insert(val);
            }
            else
            {
               print_null(proc, name, tmp_label);
            }
         }
      }
      i++;
      type_name = args->arg[i].type;
   }
   return;
}

void display_type_printers()
{
   TypePrinterEntry *entry = type_printer_for_type;
   while (entry->type_name != NULL)
   {
      printf("    %s", entry->type_name);
      if (entry->comment)
         printf(" (%s)",entry->comment);
      printf("\n");
      entry++;
   }
}

int print_mxargv(const mxProc *proc, Elf_Addr argc_addr, Elf_Addr argv, Elf_Addr argx_addr)
{
   if (! (argc_addr && argv && argx_addr))
   {
      warning("Unable to find internal process arguments.  Will print main() arguments instead.");
      printArgv++;
      return 1;
   }

   //lib/system/mdsystem/c/sysarg.c
   int argc = read_int(proc, argc_addr);
   print_argv(proc, argc, argv);

   int i=0;
   Elf_Addr argx = read_addr(proc, argx_addr);
   if (argx)
   {
      while (Elf_Addr arg = read_addr(proc, argx + i * sizeof(Elf_Addr)))
      {
         char v[256];
         read_string(proc, arg, v, sizeof(v));
         printf("arg_x[%d]='%s'\n",i,v);
         i++;
      }
   }
   printf("\n");
   return 0;
}

static void print_argv(const mxProc *proc, int argc, Elf_Addr argv)
{
   if (argc > 256)
   {
      warning("argc too big (%d).  Not printing argv", argc);
      return;
   }

   for (int i=0; i<argc; i++)
   {
      char v[256];
      Elf_Addr arg = read_addr(proc, argv + i * sizeof(Elf_Addr));
      read_string(proc, arg, v, sizeof(v));
      printf("argv[%d]='%s'\n",i,v);
   }
}

int print_main_argv(const mxProc *proc, const char *name, const char *comment, mxArguments *args)
{
   // Only print argv if we were called with -m
   if (!printArgv)
      return 0;

   if (args->count <2)
   {
      warning("Only %d arguments found for main().  Can't print arguments.", args->count);
      return 0;
   }

   print_argv(proc, args->arg[0].val.val, args->arg[1].val.val);

   return 0;
}

void print_raw1k(const mxProc *proc, const char *name, const char *comment, Elf_Addr addr)
{
#define perLine 16
   unsigned char raw[perLine];

   for (int i=0; i<1024/perLine; i++)
   {
      if (readMxProcVM(proc, addr, &raw, perLine))
      {
         printf("Unable to print 1k raw data");
         return;
      }

      //Address
      printf(FMT_ADR": ",(unsigned long)addr);

      //Hex
      for (int j=0; j<perLine; j++)
         printf("%02x ",(unsigned int)raw[j]);
      printf(" ");

      //Raw char
      for (int j=0; j<perLine; j++)
         printf("%c",raw[j]);
      printf("\n");


      addr+=perLine;
   }
}

void print_disassemble(const mxProc *proc, const char *name, const char *comment, Elf_Addr disAddr)
{
#if defined (_LP64) && (__x86_64)
   getArguments(proc,disAddr,0x0,1);
#else
   printf("Disassembly only supported on x86 64bit\n");
#endif
}

int print_corrupt_heap(const mxProc * proc, const char *name, const char *comment, mxArguments *args)
{
   static int warned=0;
   if (! warned++)
      warning("Heap memory corruption has been detected. Analysis with Purify/Valgrind is required.");

   return 1;
}

int print_jvm_full(const mxProc * proc, const char *name, const char *comment, mxArguments *args)
{
   static int warned=0;
   if (! warned++)
      warning("JVM heap may be full. Consider increasing /MXJ_JVM:-Xmx to a higher value.");

   return 1;
}

int print_libc_message(const mxProc * proc, const char *name, const char *comment, mxArguments *args)
{
   // __libc_message takes a variable number of arguments
   // First is an action - we ignore this
   // Second is a printf like format string
   // The remaining args are the format string args
   //
   if (args->count < 2) // We need at least 2 args
      return 0;

   char formatString[10240]= {0};

   read_string(proc, args->arg[1].val.val, formatString, sizeof(formatString));
   if (!formatString[0])
      return 0;

   warning("Encountered glibc message:");

   // Very simple printf style format printer
   int argNo = 0;
   char *c = formatString;

   while (*c)
   {
      if (*c == '%')
      {
         c++;
         if (args->count < ++argNo + 2)
         {
            warning("Not enough arguments for format string");
            return 0;
         }

         if (*c=='s')
         {
            char stringArg[10240]= {0};
            read_string(proc, args->arg[1+argNo].val.val, stringArg, sizeof(stringArg));
            printf("%s",stringArg);
         } 
         else
         {
            printf("== arg type %c unsupported by pmx ==",*c);
         }
         c++;
      } 
      else
      {
         printf("%c",*c);
         c++;
      }
   }

   return 1;
}

int print_strlen(const mxProc * proc, const char *name, const char *comment, mxArguments *args)
{
   if (args->count >= 1) // We can't check the type as it isn't a mangled C++ symbol
      print_string(name, "s", proc, args->arg[0].val.val);

   return 1;
}

int print_strcpy(const mxProc * proc, const char *name, const char *comment, mxArguments *args)
{
   if (args->count >= 2) // We can't check the type as it isn't a mangled C++ symbol
   {
      print_string(name, "dst", proc, args->arg[0].val.val);
      print_string(name, "src", proc, args->arg[1].val.val);
   }
   return 1;
}

int print_sprintf(const mxProc * proc, const char *name, const char *comment, mxArguments *args)
{
   if (args->count >= 2) // We can't check the type as it isn't a mangled C++ symbol
   {
      print_string(name, "dst", proc, args->arg[0].val.val);
      print_string(name, "format", proc, args->arg[1].val.val);
   }
   return 1;
}

int print_snprintf(const mxProc * proc, const char *name, const char *comment, mxArguments *args)
{
   if (args->count >= 3) // We can't check the type as it isn't a mangled C++ symbol
   {
      print_string(name, "dst", proc, args->arg[0].val.val);
      print_string(name, "format", proc, args->arg[2].val.val);
   }
   return 1;
}

int print_strcmp(const mxProc * proc, const char *name, const char *comment, mxArguments *args)
{
   if (args->count >= 2) // We can't check the type as it isn't a mangled C++ symbol
   {
      print_string(name, "s1", proc, args->arg[0].val.val);
      print_string(name, "s2", proc, args->arg[1].val.val);
   }
   return 1;
}

void setInlineMode(int mode)
{
   inlineMode=mode;
}

int getInlineMode()
{
   return inlineMode;
}
