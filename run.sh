#!/usr/bin/env bash
export LD_LIBRARY_PATH=$(pwd)/build/install/lib
./build/install/bin/main.dbg.exe "$@"