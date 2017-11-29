/*
*  Copyright Murex S.A.S., 2003-2013. All Rights Reserved.
*
*  This software program is proprietary and confidential to Murex S.A.S and its affiliates ("Murex") and, without limiting the generality of the foregoing reservation of rights, shall not be accessed, used, reproduced or distributed without the
*  express prior written consent of Murex and subject to the applicable Murex licensing terms. Any modification or removal of this copyright notice is expressly prohibited.
*/
#ifndef PMX_H
#define PMX_H

#include "mxProcUtils.h"

typedef void StackHandler(const mxProc * proc, const char *name, const char *comment, Elf_Addr value);
// Return 0 to continue processing function arguments by type, non-zero to not automatically process arguments
typedef int FunctionHandler(const mxProc * proc, const char *name, const char *comment, mxArguments *args);

typedef struct
{
   const char *type_name;
   StackHandler *stack_handler;
   const char *comment;
   bool pointer;
   bool auto_detect;
} TypePrinterEntry;

typedef struct
{
   const char *function_name;
   FunctionHandler *function_handler;
} FunctionPrinterEntry;

void display_arguments(const mxProc *proc, const char *name, mxArguments *args);
int print_mxargv(const mxProc *proc, Elf_Addr argc_addr, Elf_Addr argv, Elf_Addr argx);

void setInlineMode(int mode);
int getInlineMode();

int append_type_printers(TypePrinterEntry *ext);
void display_type_printers();

void print_arbitrary_type(const mxProc *proc, Elf_Addr addr, const char *dataType);

void print_null(const mxProc *, const char *, const char *);

void print_double(const char *function_name, const char *type, const mxProc * proc, Elf_Addr offset);
void print_double_argument(const mxProc *, const char *, const char *, Elf_Addr);
void print_double_with_decimal(const char *function_name, const char *type, mxProc * proc, Elf_Addr offset);

void print_long(const char *function_name, const char *type, const mxProc * proc, Elf_Addr offset);
void print_int(const char *function_name, const char *type, const mxProc * proc, Elf_Addr offset);

void print_string(const char *function_name, const char *type, const mxProc * proc, Elf_Addr offset, int skipIfEmpty, int maxLength);
void print_string(const char *function_name, const char *type, const mxProc * proc, Elf_Addr offset, int skipIfEmpty);
void print_string(const char *function_name, const char *type, const mxProc * proc, Elf_Addr offset);
void print_string_pointer(const char *function_name, const char *type, const mxProc * proc, Elf_Addr offset);
void print_string_argument(const mxProc *, const char *, const char *, Elf_Addr);
void print_std_string(const mxProc *, const char *, const char *, Elf_Addr);


#endif

