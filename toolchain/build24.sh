#########################################################################
#                         Toolchain Build Script                        #
#########################################################################

ROOTDIR=$PWD

GCCVER1=3.4.6
TARGETDIR1=hndtools-mipsel-uclibc-${GCCVER1}
DESTDIR1=/opt/brcm/${TARGETDIR1}

GCCVER2=4.2.4
TARGETDIR2=hndtools-mipsel-uclibc-${GCCVER2}
DESTDIR2=/opt/brcm/${TARGETDIR2}

make -C ../release/src prepk

#########################################################################

cd $ROOTDIR
rm -f .config
ln -sf config.2.4-${GCCVER1} .config
make clean; make dirclean; make V=99

cd $DESTDIR1/bin
ln -nsf mipsel-linux-uclibc-gcc-${GCCVER1} mipsel-linux-uclibc-gcc
ln -nsf mipsel-linux-uclibc-gcc-${GCCVER1} mipsel-linux-gcc-${GCCVER1}
ln -nsf mipsel-linux-uclibc-gcc-${GCCVER1} mipsel-uclibc-gcc-${GCCVER1}

#########################################################################

cd $ROOTDIR
rm -f .config
ln -sf config.2.4-${GCCVER2} .config
make clean; make dirclean; make V=99

cd $DESTDIR2/bin
ln -nsf mipsel-linux-uclibc-gcc-${GCCVER2} mipsel-linux-uclibc-gcc
ln -nsf mipsel-linux-uclibc-gcc-${GCCVER2} mipsel-linux-gcc-${GCCVER2}
ln -nsf mipsel-linux-uclibc-gcc-${GCCVER2} mipsel-uclibc-gcc-${GCCVER2}

#########################################################################

mv -f ${DESTDIR1}/bin/mipsel-linux-uclibc-gcc-${GCCVER1} ${DESTDIR2}/bin/
mv -f ${DESTDIR1}/include/c++/${GCCVER1} ${DESTDIR2}/include/c++/
mv -f ${DESTDIR1}/lib/gcc/mipsel-linux-uclibc/${GCCVER1} ${DESTDIR2}/lib/gcc/mipsel-linux-uclibc/
mv -f ${DESTDIR1}/libexec/gcc/mipsel-linux-uclibc/${GCCVER1} ${DESTDIR2}/libexec/gcc/mipsel-linux-uclibc/
mv -f ${DESTDIR1}/info/cppinternals.info ${DESTDIR2}/info/
mv -f ${DESTDIR1}/info/gccint.info ${DESTDIR2}/info/

cd $DESTDIR2/bin
ln -nsf mipsel-linux-uclibc-gcc-${GCCVER1} mipsel-linux-gcc-${GCCVER1}
ln -nsf mipsel-linux-uclibc-gcc-${GCCVER1} mipsel-uclibc-gcc-${GCCVER1}

#########################################################################

cd /opt/brcm
rm -f hndtools-mipsel-linux
rm -f hndtools-mipsel-uclibc

mkdir -p K24
rm -rf K24/hndtools-mipsel-uclibc-${GCCVER2}
mv -f hndtools-mipsel-uclibc-${GCCVER2} K24/

ln -nsf K24/hndtools-mipsel-uclibc-${GCCVER2} hndtools-mipsel-linux
ln -nsf K24/hndtools-mipsel-uclibc-${GCCVER2} hndtools-mipsel-uclibc

rm -rf ${DESTDIR1}

cd $ROOTDIR
