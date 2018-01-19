bin_PROGRAMS = $(top_builddir)/bin/pmx


__top_builddir__bin_pmx_SOURCES = main.c \
pmx.c \
mxProcUtils.c 

if LINUX
__top_builddir__bin_pmx_SOURCES += mxProcUtils_linux.c
endif
if SOLARIS
__top_builddir__bin_pmx_SOURCES += mxProcUtils_SunOS.c
endif

if X86
__top_builddir__bin_pmx_SOURCES += mxProcUtils_x86.c
endif
if SPARC
__top_builddir__bin_pmx_SOURCES += mxProcUtils_sparc.c
endif

__top_builddir__bin_pmx_CFLAGS = -I$(srcdir)/../include -fPIC 

if LINUX
__top_builddir__bin_pmx_CFLAGS += -pie  -Wl,-E
endif

__top_builddir__bin_pmx_LDADD = -ldl 

if SOLARIS
__top_builddir__bin_pmx_LDADD += -ldemangle
endif
