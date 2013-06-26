#!/bin/bash
#
# Miscellaneous steps to prepare the root filesystem
#
# Copyright (C) 2012, Broadcom Corporation. All Rights Reserved.
# 
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
# 
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
# OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#
# $Id: rootprep.sh,v 1.16 2008-11-26 07:38:16 $
#

ROOTDIR=$PWD

mkdir -p -m 0755 dev
mkdir -p -m 0755 proc
mkdir -p -m 0755 sys
mkdir -p -m 0755 jffs
mkdir -p -m 0755 cifs1
mkdir -p -m 0755 cifs2
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

