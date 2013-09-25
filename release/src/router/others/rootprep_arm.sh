#!/bin/bash

ROOTDIR=$PWD

mkdir -p -m 0755 dev
mkdir -p -m 0755 proc
mkdir -p -m 0755 sys
mkdir -p -m 0755 jffs
mkdir -p -m 0755 cifs1
mkdir -p -m 0755 cifs2
mkdir -p -m 0755 sysroot
ln -sf tmp/opt opt

# tmp
mkdir -p -m 0755 tmp
ln -sf tmp/var var
ln -sf tmp/media media
(cd $ROOTDIR/usr && ln -sf ../tmp)

# etc
rm -rf etc
ln -sf tmp/etc etc
echo "/lib" > etc/ld.so.conf
echo "/usr/lib" >> etc/ld.so.conf
/sbin/ldconfig -r $ROOTDIR

# !!TB
mkdir -p -m 0755 mmc
mkdir -p -m 0755 usr/local
ln -sf /tmp/share usr/share
ln -sf /tmp/share usr/local/share

ln -sf tmp/mnt mnt
ln -sf tmp/home home
ln -sf tmp/home/root root
(cd usr && ln -sf ../tmp)

# !!TB
ln -sf /tmp/var/wwwext www/ext
ln -sf /tmp/var/wwwext www/user
ln -sf /www/ext/proxy.pac www/proxy.pac
ln -sf /www/ext/proxy.pac www/wpad.dat

