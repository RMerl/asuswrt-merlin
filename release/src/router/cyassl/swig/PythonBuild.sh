#!/bin/bash
echo
swig -python cyassl.i
pythonIncludes=`python-config --includes`
pythonLibs=`python-config --libs`
gcc -c -fpic cyassl_wrap.c -I$pythonIncludes -I/usr/local/cyassl/include -DHAVE_CONFIG_H
gcc -c -fpic cyassl_adds.c -I/usr/local/cyassl/include
gcc -shared -flat_namespace  cyassl_adds.o  cyassl_wrap.o -lcyassl -L/usr/local/cyassl/lib $pythonLibs -o _cyassl.so
python runme.py
