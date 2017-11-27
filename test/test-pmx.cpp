#include <stdio.h>
#include <iostream>
#include <cstring>
#include <signal.h>

#include "structFOO.h"

using namespace std;

int foo( std::string t, FOO *p, char *s, int a, int *b, int c )
{
	printf("t='%s', s='%s'\n", t.c_str(), s);
	printf( "src='%s', contains 'hello' ? %s\n", p->str, strncmp( p->str, "hello", 5 ) ? "No" : "Yes" );
	int *x = NULL; 	
	printf("p(NULL) = %d\n", *x );	
	return 0;
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

