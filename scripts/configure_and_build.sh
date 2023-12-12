#!/bin/bash

# Reconfigures the cmake build directory and triggers a build. To be executed
# from a docker container where the LoAT root directory is mounted at
# /LoAT/. Reconfiguration is necessary to get the information whether the
# working tree is clean or not.

mkdir -p /swine/build-debug
cd /swine/build-debug
cmake -DCMAKE_BUILD_TYPE=Debug ../
make -j

cd /swine

/swine/scripts/repack-static-lib.sh\
    libswine.a\
    /swine/build-debug/libswine.a\
    /usr/local/lib/libz3.a\
    /usr/local/lib/libpoly.a\
    /usr/local/lib/libpolyxx.a\
    /usr/local/lib/libsmt-switch*
    # /usr/local/lib/libcudd.a
    # /usr/local/lib/libyices.a
