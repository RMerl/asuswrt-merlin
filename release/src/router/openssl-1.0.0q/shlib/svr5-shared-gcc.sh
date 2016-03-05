#!/usr/bin/sh

major="0"
minor="9.7b"

slib=libssl-1.0.0q
sh_slib=$slib.so.$major.$minor

clib=libcrypto-1.0.0q
sh_clib=$clib.so.$major.$minor

FLAGS="-O3 -DFILIO_H -fomit-frame-pointer -pthread"
SHFLAGS="-DPIC -fPIC"

touch $sh_clib
touch $sh_slib

echo collecting all object files for $clib.so
OBJS=
find . -name \*.o -print > allobjs
for obj in `ar t libcrypto-1.0.0q.a`
do
	OBJS="$OBJS `grep $obj allobjs`"
done

echo linking $clib.so
gcc -G -o $sh_clib -h $sh_clib $OBJS -lnsl -lsocket

rm -f $clib.so
ln -s $sh_clib $clib.so

echo collecting all object files for $slib.so
OBJS=
for obj in `ar t libssl-1.0.0q.a`
do
	OBJS="$OBJS `grep $obj allobjs`"
done

echo linking $slib.so
gcc -G -o $sh_slib -h $sh_slib $OBJS -L. -lcrypto-1.0.0q

rm -f $slib.so
ln -s $sh_slib $slib.so

mv libRSAglue.a libRSAglue.a.orig
mv libcrypto-1.0.0q.a  libcrypto-1.0.0q.a.orig
mv libssl-1.0.0q.a     libssl-1.0.0q.a.orig

