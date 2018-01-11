bin_PROGRAMS = $(top_builddir)/bin/testpmx

__top_builddir__bin_testpmx_SOURCES = test-pmx.cpp \
lib/structFOO.h \
lib/types_FOO.h 

__top_builddir__bin_testpmx_CXXFLAGS = -I$(srcdir)/../include -I$(srcdir)/lib  -L. -Wno-invalid-off -Wall -Werror -g -fPIC -masm=intel -O1 -fno-omit-frame-pointer -DFSL_MUREX_BUILD -D_REENTRANT -DUNIX -DNDEBUG -DMUREX_MBL_SP_DISABLE_THREADS -DMUREX_MBL_SP_USE_QUICK_ALLOCATOR -lrt -ldl -lm

