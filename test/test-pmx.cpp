#include <stdio.h>
#include <iostream>
#include <cstring>
#include <signal.h>

#include "structFOO.h"
#include "pmxsupport.h"
//#include "pmx_stackcontext.hpp"

using namespace std;

int foo( std::string t, FOO *p, char *s, int a, int *b, int c );
int bar(FOO *f, const char *cmd, int flag, char *tabname, int filterType, const char *condition, bool fWithCond, int exceptionRule);
void dummyFunc(double a, bool b, float c, unsigned char d, signed e, long long f, unsigned short int g, long double h);

int foo( std::string t, FOO *p, char *s, int a, int *b, int c )
{
	printf("t='%s', s='%s'\n", t.c_str(), s);
	printf( "src='%s', contains 'hello' ? %s\n", p->str, strncmp( p->str, "hello", 5 ) ? "No" : "Yes" );

	int ret = bar(p, (const char *)"hello", 0x0a0b0c0d, (char *)"murex", 128, NULL, true, -1);
	printf("ret = %d\n", ret);
	return 0;
}

int bar(FOO *f, const char *cmd, int flag, char *tabname, int filterType, const char *condition, bool fWithCond, int exceptionRule)
{
#if 1
	PMX_INSTRUMENT(f, cmd, flag, tabname, filterType, condition, fWithCond, exceptionRule);
#else // debug -- expand the PMX_INSTRUMENT macro
   volatile struct {
      unsigned int start_tag;
      unsigned long pmx_f;
      unsigned long pmx_cmd;
      unsigned long pmx_flag;
      unsigned long pmx_tabname;
      unsigned long pmx_filterType;
      unsigned long pmx_condition;
      unsigned long pmx_fWithCond;
      unsigned long pmx_exRule;
      unsigned int end_tag;
   } __attribute__((unused))
   mx_instrumentation = {0x0,
      *(unsigned long *)&f,
      *(unsigned long *)&cmd,
      *(unsigned long *)&flag,
      *(unsigned long *)&tabname,
      *(unsigned long *)&filterType,
      *(unsigned long *)&condition,
      *(unsigned long *)&fWithCond,
      *(unsigned long *)&exceptionRule,
      0x0};

   mx_instrumentation.start_tag = 0xCAFEF00D;
   mx_instrumentation.end_tag = 0xFABABBA0;
#endif

   dummyFunc( 1, true, 3.5, 'a', 5, 6L, 7, 8);

	return 0;
}

void dummyFunc(double a, bool b, float c, unsigned char d, signed e, long long f, unsigned short int g, long double h)
{
   PMX_INSTRUMENT(a, b, c, d, e, f, g, h);

   long double xx = a * a + b * b + c + d + e + f + g + h;
   long double yy = a * b + a / b;

   printf("xx = %LF, yy = %LF\n", xx, yy);
   int *x = NULL;
	printf("p(NULL) = %d\n", *x );	
}

int main( int argc, char **argv )
{
	int a = 3, b = 2, c = 1;
	char test[] = "fixed test string";
	FOO fp;
	fp.a = &a;
	fp.b = &b;
	fp.c = a * b;
	fp.al = 12345678L;
	fp.alp = &fp.al;
	fp.ad = 12.3456789;
	fp.adp = &fp.ad;
	fp.str = argv[1];

	foo( std::string(argv[0]), &fp, test, a, &b, c );
	return 0;
}	

