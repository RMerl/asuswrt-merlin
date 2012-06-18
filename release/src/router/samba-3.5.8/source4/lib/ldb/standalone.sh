#!/bin/sh

cd ../replace
make clean

cd ../talloc
make clean

cd ../tdb
make clean

cd ../events
make clean

cd ../ldb
make clean

./autogen.sh

rm -fr build
mkdir build
cd build

../configure $*
make dirs
make all

cd ..
