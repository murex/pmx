/*******************************************************************************
*
* Copyright (c) {2003-2018} Murex S.A.S. and its affiliates.
* All rights reserved. This program and the accompanying materials
* are made available under the terms of the Eclipse Public License v1.0
* which accompanies this distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*
*******************************************************************************/

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

void print_null(const mxProc *, const char *, const char *);
void print_void_pointer(const mxProc *, const char *, const char *, Elf_Addr);
void print_raw1k(const mxProc *, const char *, const char *, Elf_Addr);
void print_disassemble(const mxProc *, const char *, const char *, Elf_Addr);
void print_arbitrary_type(const mxProc *proc, Elf_Addr addr, const char *dataType);
int print_corrupt_heap(const mxProc * proc, const char *name, const char *comment, mxArguments *args);
int print_jvm_full(const mxProc * proc, const char *name, const char *comment, mxArguments *args);
int print_libc_message(const mxProc * proc, const char *name, const char *comment, mxArguments *args);

void print_int(const char *function_name, const char *type, const mxProc * proc, Elf_Addr offset);
void print_int_argument(const mxProc *, const char *, const char *, Elf_Addr);
void print_int_pointer(const mxProc *, const char *, const char *, Elf_Addr);
void print_long(const char *function_name, const char *type, const mxProc * proc, Elf_Addr offset);

void print_double(const char *function_name, const char *type, const mxProc * proc, Elf_Addr offset);
void print_double_pointer(const mxProc *, const char *, const char *, Elf_Addr);
void print_double_argument(const mxProc *, const char *, const char *, Elf_Addr);
void print_double_with_decimal(const char *function_name, const char *type, mxProc * proc, Elf_Addr offset);

void print_string(const char *function_name, const char *type, const mxProc * proc, Elf_Addr offset, int skipIfEmpty, int maxLength);
void print_string(const char *function_name, const char *type, const mxProc * proc, Elf_Addr offset, int skipIfEmpty);
void print_string(const char *function_name, const char *type, const mxProc * proc, Elf_Addr offset);
void print_string_pointer(const char *function_name, const char *type, const mxProc * proc, Elf_Addr offset);
void print_string_argument(const mxProc *, const char *, const char *, Elf_Addr);
void print_std_string(const mxProc *, const char *, const char *, Elf_Addr);

static void print_argv(const mxProc *proc, int argc, Elf_Addr argv);
int print_main_argv(const mxProc *proc, const char *name, const char *comment, mxArguments *args);

int print_strlen(const mxProc * proc, const char *name, const char *comment, mxArguments *args);
int print_strcpy(const mxProc * proc, const char *name, const char *comment, mxArguments *args);
int print_sprintf(const mxProc * proc, const char *name, const char *comment, mxArguments *args);
int print_snprintf(const mxProc * proc, const char *name, const char *comment, mxArguments *args);
int print_strcmp(const mxProc * proc, const char *name, const char *comment, mxArguments *args);

#endif  // PMX_H

