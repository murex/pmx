AC_INIT(pmx, 1.0, pmx-support@murex.com)

LT_INIT

AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE

AC_CANONICAL_HOST

case $host in
*86*linux*)
	LINUX=yes
	X86=yes
	#AC_DEFINE(__linux)
	;;
*86*solaris*)
	SOLARIS=yes
	X86=yes
	;;
*sparc*sun*)
	SOLARIS=yes
	SPARC=yes
	;;
*)
	;;
esac

AM_CONDITIONAL(LINUX, test x$LINUX = xyes)
AM_CONDITIONAL(SOLARIS, test x$SOLARIS = xyes)
AM_CONDITIONAL(X86, test x$X86 = xyes)
AM_CONDITIONAL(SPARC, test x$SPARC = xyes)

#if test -z $CXXFLAGS; then
#    CXXFLAGS='-O0 -g -Wall'
#fi

AC_PROG_CXX
AC_PROG_CC
AM_PROG_CC_C_O

AC_OUTPUT(Makefile src/Makefile test/Makefile test/lib/Makefile)
