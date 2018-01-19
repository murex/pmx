bin_PROGRAMS = $(top_builddir)/bin/testpmx

__top_builddir__bin_testpmx_SOURCES = test-pmx.cpp \
lib/structFOO.h \
lib/types_FOO.h 

__top_builddir__bin_testpmx_CXXFLAGS = -I$(srcdir)/../include -I$(srcdir)/lib  -L. -Wno-invalid-off -Wall -Werror -g -std=c++11 -fPIC -O1 -lrt -ldl -lm

if LINUX
__top_builddir__bin_testpmx_CXXFLAGS += -fno-omit-frame-pointer 
endif
