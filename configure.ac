AC_INIT(pmx, 1.0, pmx-support@murex.com)

AC_DEFINE(PMX_VERSION_MAJOR, "1", "pmx major version")
AC_DEFINE(PMX_VERSION_MINOR, "0", "pmx minor version")
AC_DEFINE(PMX_BUILD_ID, "2018", "pmx build id")

: ${CFLAGS=""}
: ${CXXFLAGS=""}

LT_INIT

AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE

AC_CANONICAL_HOST

case $host in
*86*linux*)
	LINUX=yes
	X86=yes
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

AC_PROG_CXX
AC_PROG_CC
AM_PROG_CC_C_O

AC_OUTPUT(Makefile src/Makefile test/Makefile test/lib/Makefile)
