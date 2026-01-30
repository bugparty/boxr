#!/usr/bin/env bash
export LD_LIBRARY_PATH=$HOME/anaconda3/lib:$(pwd)/build/install/lib:$LD_LIBRARY_PATH

# Add all plugin subdirectories to LD_LIBRARY_PATH
for dir in $(pwd)/build/plugins/*; do
    if [ -d "$dir" ]; then
        export LD_LIBRARY_PATH=$dir:$LD_LIBRARY_PATH
    fi
done

./build/main.dbg.exe "$@"