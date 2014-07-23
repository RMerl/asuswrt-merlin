#!/bin/sh
#
# Building OSX fat binaries is easy.
#
# - start in a clean directory.
# - copy the shell script below to a file and edit the file to your needs.
#
# 1.) modify export GEOIP_ARCH='-arch i386 -arch x86_64 -arch ppc -arch ppc64'
# to include all architectures you need.
# 2.) add whatever you want to the ./configure line.
# 3.) execute the script.
# 4.) do a 'make install'
#
#
# make clean or make distclean before building this
#
# tell systems before leopard that we like to build for 10.5 or higher
# with MACOSX_DEPLOYMENT_TARGET=10.5
# starting with leopard we have to add -mmacosx-version-min=10.5
# to the CFLAGS and export MACOSX_DEPLOYMENT_TARGET!?

##  for tiger, leopard and snow leopard you might use this
## export GEOIP_ARCH='-arch i386 -arch x86_64 -arch ppc -arch ppc64'
## export MACOSX_DEPLOYMENT_TARGET=10.4
## export LDFLAGS=$GEOIP_ARCH
## export CFLAGS="-mmacosx-version-min=10.4 -isysroot /Developer/SDKs/MacOSX10.4u.sdk $GEOIP_ARCH"

# here we go for leopard and snow leopard
#export GEOIP_ARCH='-arch i386 -arch x86_64 -arch ppc'
#export MACOSX_DEPLOYMENT_TARGET=10.5
#export LDFLAGS=$GEOIP_ARCH
#export CFLAGS="-g -mmacosx-version-min=10.5 -isysroot /Developer/SDKs/MacOSX10.5.sdk $GEOIP_ARCH"
#./configure --disable-dependency-tracking
#perl -i.bak -pe'/^archive_cmds=/ and !/\bGEOIP_ARCH\b/ and s/-dynamiclib\b/-dynamiclib \\\$(GEOIP_ARCH)/' ./libtool
#make

# and this is lion with the new xcode
#P=`xcode-select -print-path`
#export GEOIP_ARCH='-arch i386 -arch x86_64'
#export MACOSX_DEPLOYMENT_TARGET=10.7
#export LDFLAGS=$GEOIP_ARCH
#export CFLAGS="-g -mmacosx-version-min=10.7 -isysroot $P/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.7.sdk $GEOIP_ARCH"
#./configure --disable-dependency-tracking
#make

# and this is lion with the new xcode
P=`xcode-select -print-path`
export GEOIP_ARCH='-arch x86_64'
export MACOSX_DEPLOYMENT_TARGET=10.9
export LDFLAGS=$GEOIP_ARCH
export CFLAGS="-g -mmacosx-version-min=10.9 -isysroot $P/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.9.sdk $GEOIP_ARCH"
./configure --disable-dependency-tracking
make
