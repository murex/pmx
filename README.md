# PMX 

`pmx` is a tool to assist with analysing process or core file of ELF format.

## Supported Architecture

* [x] Linux x86_64
* [x] Linux i686
* [ ] Solaris x86

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

Full command options can be found from `pmx -h` command::

    ./pmx -h
    usage: pmx [mode]... [option]... [binary] {pid|core}[/lwps]
    
      binary: The name of the binary corresponding to the core/pid.
              This will be automatically detected if omitted.
      lwps:   The lwps/thread to analyse.  By default the first thread is analysed.
              To analyse all lwps/threads, use the --all-threads.
    
    Standard Modes:
      --extract, -g            Extract known data structures from stack. (default)
      --pargs, -m              Print process arguments.
      --pstack, -s             Print stack trace.  Implies --extract.
      --pldd, -b               Print loaded libraries.
      --all, -e                The same as --extract --pargs --pstack --pldd.
    
    Advanced Modes:
      --pmap, -c               Print memory map of process.
      --show-types, -t         Print supported data types.
      --raw-stack=n, -r n      Print n words from of the raw stack.
      --address=addr, -x addr  Print data structure at addr.  Use with --type.
                               addr can be a hex address (0x1234) or a symbol.
    
    Options:
      --inline, -i             Print all data to stdout rather than creating files.
      --force, -k              Run even if the binary doesn't match the core.
      --args=n, -a n           Print n (default 8) arguments for functions when we
                               don't know better.  Use with --pstack.
      --type=type, -d type     Type of data structure printed by --address.
                               Use -t to list supported types.  Default is RAW1K.
      --all-threads, -f        Process all threads, rather than just the first.
      --sysroot=path, -l path  Use path as system root for loading libraries with
      --libext=path, -L path   Path to customized type checker, default is libpmxext.so.
                               absolute references. Like 'set sysroot' in gdb.
      --output-prefix=path, -p path
                               Use path as the prefix for output files.
                               If unspecified, the core file name is used.
      --corrupt-stack=n, -j n  Search this many words for a valid frame in the case
                               of stack corruption.  Default 200.
      --verbose, -v            Print pmx debugging/troubleshooing information.


## Extending `pmx` for Customized Data Types


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

