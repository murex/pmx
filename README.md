# PMX

`pmx` is a tool to assist with analysing process or core file of ELF format.

## Supported Architecture

- :white_check_mark: Linux x86_64 
- :white_check_mark: Linux i686

## Features

* Extract stack trace from core file and live running process.
* Parse and print function arguments.
* Allow extension with a plugable shared library for user-defined complex data types.
* Features of `pstack`, `pargs`, `pmap`, `pldd` on Solaris within one binary.
* Work with corrupted stack when `gdb` fails to print stack trace. 
* Allow instrumentation of interested functions.


## Usage

`pmx` works with both core file or running process. To print the stack, run `pmx -s core` 
on core file and `pmx -s pid` on running process. The first thread stack will be printed.
It is equivalent to  `pmx -s core/1` or `pmx -s pid/1`.
To see all thread, use the `--all-threads` option.

`pmx` assumes the process runs in current directory. To load dependent libraries from another 
directory, use the `-l` or `--sysroot` option.

Full command options can be found from `pmx -h` command output.


## Extending `pmx` for Customized Data Types

`pmx` will parse and extract all primitive data types in C/C++. However, it's possible to
extend `pmx` to parse composite data types and classes with an extenion shared library. 

An extension should be define an `TypePrinterEntry` array named *type_printer_extension*. 
When `pmx` starts, it'll find the `libpmxext.so` library (configurable with `-L` option) and 
append the customized type printer to the supported type list. Refer to 
[test/lib/pmxext.cpp]([test/lib/pmxext.cpp) and the accompanying [type_FOO.cpp](test/lib/type_FOO.cpp)
files for detail.

## Working with Optimized Code

`pmx` relies on the arguments to be passed onto stack. When code is compiled with 
optimization, the compiler may optimize out arguments for efficiency. `pmx` provides a macro
to instrument the function and save a copy of the arguments on the stack for easy argument 
extraction.

To instrument a function, call the `PMX_INSTRUMENT()` macro with all the argument 
at the very first line of the function.

```cpp
#include "pmxsupport.h"

// Example of standard C function, or static C++ method. Up to 9 arguments are supported
static int foo(const char *a, int b, char *c, int d, const char *e)
{
   PMX_INSTRUMENT(a, b, c, d, e);
   // rest of function 
}

//Example of a (non-static) C++ method.  Up to 8 arguments are supported.
voi bar(void *a, int b, double c, char d)
{
   // Note that 'this' is automatically added to the arguments by the macro, and can be analysed by pmx.
   PMX_INSTRUMENT_METHOD(a, b, c, d);

   // Rest of method
}
```

If you are interested in what the macro translates to, the instrumentation of `bar()` would expand to:

```cpp
void *vthis = this; // Syntactic work around as we can't dereference 'this' directly.  No impact on compiled code.
volatile struct {  // must be volatile so it isn't optimised away.
    unsigned long start_tag;
    void * pmx_vthis;
    void *pmx_a;
    int pmx_b;
    double pmx_c;
    char pmx_d;
    unsigned long end_tag;
  } mx_instrumentation =
  {
    0xCAFEF00D, // Magic number start tag
    vthis,
    a,
    b,
    c,
    d,
    0xFABABBA0 // Magic number end tag
  };
mx_instrumentation.start_tag=0xCAFEF00D; 
mx_instrumentation.end_tag=0xFABABBA0;
```

## Compile and Test

You will need `autotools` to compile the project:

```bash
git clone git@github.com:murex/pmx.git
cd pmx
./autogen.sh 
mkdir build
# change compiler as appropriate
../configure CC="g++492" CXX="g++492"
make
```

This will build `pmx` binary, a test program `testpmx` which could generate a core file, 
and a shared library plugin customized type printer defined in `test/lib` diretory.

To test:

```bash
cd bin
./testpmx "hello pmx test"
# create a symbolic link to the shared library
../test/lib/.libs/libpmxext-0.1.so.0.0.1 libpmxext.so
./pmx -s core.xxx
```

## License

`pmx` and the accompanying materials are made available under the terms of the Eclipse 
Public License v1.0 ([here](LICENSE.txt)) which accompanies this ditribution, and is 
available at http://www.eclipse.org/legal/epl-v10.html.

## Contributing 

The master branch of this repository contains the latest stable changes. Pull requests 
should be submitted against the latest head of master. See [here](CONTRIBUTING.md) for 
contributing agreement.

## Contact

pmx-support@murex.com

