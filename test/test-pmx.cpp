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
#include <iostream>
#include <cstring>
#include <signal.h>

#include "structFOO.h"
#include "pmxsupport.h"

using namespace std;

int foo( std::string t, FOO *p, char *s, int a, int *b, int c );
int bar(FOO *f, const char *cmd, int flag, char *tabname, int filterType, const char *condition, bool fWithCond, int exceptionRule);
void dummyFunc(double a, bool b, float c, unsigned char d, signed e, long long f, unsigned short int g, long double h);
void singleArg(int a);

void singleArg(int a)
{
   PMX_INSTRUMENT(a);
   int b = a;
   printf("b = %d\n", b);
   int *x = NULL;
	printf("p(NULL) = %d\n", *x );	
}

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
	PMX_INSTRUMENT(f, cmd, flag, tabname, filterType, condition, fWithCond, exceptionRule);

   dummyFunc( 1, true, 3.5, 'a', 5, 6L, 7, 8);

	return 0;
}

void dummyFunc(double a, bool b, float c, unsigned char d, signed e, long long f, unsigned short int g, long double h)
{
   PMX_INSTRUMENT(a, b, c, d, e, f, g, h);

   long double xx = a * a + b * b + c + d + e + f + g + h;
   long double yy = a * b + a / b;

   printf("xx = %LF, yy = %LF\n", xx, yy);
   singleArg(e);
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

