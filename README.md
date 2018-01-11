# PMX 

`pmx` is a utility that parses stack trace of a live process or core file. 

## Supported Architecture

* [yes] Linux x86_64
* [Todo] Solaris x86

## Features

* TODO



## Compile and Test

You will need `autotools` to compile the project:

```
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

```
cd bin

./testpmx "hello pmx test"

# create a symbolic link to the shared library
../test/lib/.libs/libpmxext-0.1.so.0.0.1 libpmxext.so

./pmx -s core.xxx
```

## License

`pmx` is released under Eclipse Public License v1.0 ([here](LICENSE.txt])).

## Contributing 

The master branch of this repository contains the latest stable changes. Pull requests should be submitted against the latest head of master. See [here](CONTRIBUTING.md) for contributing agreement.

## Contact

pmx-support@murex.com

