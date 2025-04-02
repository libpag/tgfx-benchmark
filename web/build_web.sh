#!/bin/bash

# Check the first argument to determine the build type
if [ "$1" == "wasm-mt" ]; then
    # Source the emsdk environment
    source ../third_party/emsdk/emsdk_env.sh
    if [ "$2" == "debug" ]; then
        node script/cmake.demo.js -a wasm-mt --debug
    else
        node script/cmake.demo.js -a wasm-mt
    fi
    # Run the build commands
    npm run build:tgfx
    rollup -c ./script/rollup.demo.js --environment ARCH:$1
elif [ "$1" == "wasm" ]; then
    # Source the emsdk environment
    source ../third_party/emsdk/emsdk_env.sh
    if [ "$2" == "debug" ]; then
        node script/cmake.demo.js -a wasm --debug
    else
        node script/cmake.demo.js -a wasm
    fi
    # Run the build commands
    npm run build:tgfx
    rollup -c ./script/rollup.demo.js --environment ARCH:$1
elif [ "$1" == "clean:tgfx" ]; then
    rimraf lib/ types/
elif [ "$1" == "clean" ]; then
    rimraf lib/ types/
    rimraf demo/build demo/wasm demo/wasm-mt demo/.*.md5 demo/*.js demo/*.map demo/cmake-build-*
elif [ "$1" == "build:tgfx" ]; then
    rimraf lib/ types/
    tsc -p ./tsconfig.type.json
    rollup -c ./script/rollup.tgfx.js
else
    echo "Invalid type."
    exit 1
fi