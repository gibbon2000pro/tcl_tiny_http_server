#! /bin/bash

cd tcl_http
rm -rf build
mkdir build
cd build
cmake ..
make

cd ../..
cp tcl_http/build/libtclhttp.so .
