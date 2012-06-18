#!/bin/sh
# LIC: GPL
#***********************************************************************
#
# go
#
# Quick-start shell script to set up PPPoE
#
# Copyright (C) 2000 Roaring Penguin Software Inc.
#
# $Id$
#***********************************************************************

# Figure out directory of script
MYDIR=`dirname $0`
cd $MYDIR/src

echo "Running ./configure..."
./configure
if [ "$?" != 0 ] ; then
    echo "Oops!  It looks like ./configure failed."
    exit 1
fi

echo "Running make..."
make
if [ "$?" != 0 ] ; then
    echo "Oops!  It looks like make failed."
    exit 1
fi

echo "Running make install..."
make install

if [ "$?" != 0 ] ; then
    echo "Oops!  It looks like make install failed."
    exit 1
fi

for i in a a a a a a a a a a a a a a a a a a a a a a a a a a a a ; do
    echo ""
done

sh ../scripts/pppoe-setup
