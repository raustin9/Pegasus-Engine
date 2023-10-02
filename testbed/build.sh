#!/bin/bash
# Build the testbed

set echo on

mkdir -p ../bin

# Get list of all .c files
cFilenames=$(find . -type f -name "*.c")

assembly="testbed"
cflags="-g -fdeclspec -fPIC"

includes="-Isrc -I../engine/src"
ldflags="-L../bin/ -lengine -Wl,-rpath,./bin/" # allows us to load at runtime
defines="-D_DEBUG -DPIMPORT"

echo "Building $assembly..."
echo clang $cFilenames $cflags -o ../bin/$assembly $defines $includes $ldflags
clang $cFilenames $cflags -o ../bin/$assembly $defines $includes $ldflags
