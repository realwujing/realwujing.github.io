#! /bin/bash

set -ex

rm bcc/build -rf
mkdir bcc/build
cd bcc/build
cmake ..
make -j8
make install
cmake -DPYTHON_CMD=python3 .. # build python3 binding
pushd src/python/
make
#make install
popd
