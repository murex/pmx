#include "structFOO.h"
#include "types_FOO.h"
#include "pmx.h"
#include <stddef.h>

void print_struct_FOO(const mxProc * proc, const char *name, const char *comment, Elf_Addr value)
{
	print_int(name, "*a", proc, value + offsetof(struct FOO, a));
	print_int(name, "*b", proc, value + offsetof(struct FOO, b));
	print_int(name, "c", proc, value + offsetof(struct FOO, c));
	print_long(name, "al", proc, value + offsetof(struct FOO, al));
	print_long(name, "*alp", proc, value + offsetof(struct FOO, alp));
	print_double(name, "ad", proc, value + offsetof(struct FOO, ad));
	print_double(name, "*adp", proc, value + offsetof(struct FOO, adp));
	print_string(name, "str", proc, read_addr(proc, value + offsetof(struct FOO, str)));
}
