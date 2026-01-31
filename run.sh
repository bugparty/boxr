#!/usr/bin/env bash
# Add Nix libraries required by the build
export NIX_GLEW_LIB=/nix/store/c6qckjiz2f3z8dgrixfrlxnvn6dx7cxl-glew-2.2.0/lib
export NIX_SPDLOG_LIB=/nix/store/k245p0ldcajx2nzbcxn96xvxkg6pbcqy-spdlog-1.8.5/lib
export NIX_FMT_LIB=/nix/store/9yxq8kwj6zpvi4nvrzs6d5ragk5vgaxj-fmt-7.1.3/lib
export NIX_OPENCV_LIB=/nix/store/ll3zggn5ffx24za2m48pmk1incnsck1z-opencv-4.5.2/lib

export LD_LIBRARY_PATH=$(pwd)/build/install/lib:$NIX_GLEW_LIB:$NIX_SPDLOG_LIB:$NIX_FMT_LIB:$NIX_OPENCV_LIB:/usr/lib64:$LD_LIBRARY_PATH

echo "Debug: LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
ls -l $NIX_OPENCV_LIB/libopencv_gapi.so.4.5

./build/install/bin/main.dbg.exe "$@"