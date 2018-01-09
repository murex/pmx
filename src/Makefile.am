bin_PROGRAMS = $(top_builddir)/bin/pmx

AM_CXXFLAGS = -I$(top_builddir)/include

__top_builddir__bin_pmx_SOURCES = main.c \
pmx.c \
mxProcUtils.c \
mxProcUtils_linux.c \
mxProcUtils_x86.c  

__top_builddir__bin_pmx_CFLAGS = -I$(srcdir)/../include -fPIC -pie -Wl,-E
__top_builddir__bin_pmx_LDADD = -ldl
