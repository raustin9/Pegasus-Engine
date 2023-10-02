#!/bin/bash
# Build the engine

set echo on
mkdir -p ../bin

# Get list of all .c files
cFilenames=$(find . -type f -name "*.c")

assembly="engine"
cflags="-g -shared -fdeclspec -fPIC"

includes="-Isrc -I$VULKAN_SDK/include"
ldflags="-lvulkan -lxcb -lX11 -lX11-xcb -lxkbcommon -L$VULKAN_SDK/lib -L/usr/X11/lib"
defines="-D_DEBUG -DPEXPORT"

echo "Building $assembly..."
clang $cFilenames $cflags -o ../bin/lib$assembly.so $defines $includes $ldflags
