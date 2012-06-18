#########################################################################
#                         Toolchain Build Script                        #
#########################################################################

GCCVER=4.2.4

ROOTDIR=$PWD
TARGETDIR=hndtools-mipsel-uclibc-${GCCVER}
DESTDIR=/opt/brcm/${TARGETDIR}

make -C ../release/src-rt prepk

rm -f .config
ln -sf config.2.6-${GCCVER} .config
make clean; make dirclean; make V=99

cd $DESTDIR/bin
ln -nsf mipsel-linux-uclibc-gcc-${GCCVER} mipsel-linux-uclibc-gcc
ln -nsf mipsel-linux-uclibc-gcc-${GCCVER} mipsel-linux-gcc-${GCCVER}
ln -nsf mipsel-linux-uclibc-gcc-${GCCVER} mipsel-uclibc-gcc-${GCCVER}

cd /opt/brcm
rm -f hndtools-mipsel-linux
rm -f hndtools-mipsel-uclibc

mkdir -p K26
rm -rf K26/hndtools-mipsel-uclibc-${GCCVER}
mv -f hndtools-mipsel-uclibc-${GCCVER} K26/

ln -nsf K26/hndtools-mipsel-uclibc-${GCCVER} hndtools-mipsel-linux
ln -nsf K26/hndtools-mipsel-uclibc-${GCCVER} hndtools-mipsel-uclibc

cd $ROOTDIR
