
all: pkg

INSTROOT=`pwd`/tmp
PKGNAME=bonnie++

../Makefile:
	( cd .. ; ./configure --prefix=/usr --mandir=`pwd`/sun/tmp/usr/share/man )

pkg: ../Makefile
	make -C .. WFLAGS="-Wall -W -Wshadow -Wpointer-arith -Wwrite-strings"
	make -C .. prefix=${INSTROOT}/usr install
	pkgmk -o -r ${INSTROOT}
	pkgtrans -s spool ${PKGNAME}.pkg ${PKGNAME}
	@echo
	@echo "/var/spool/pkg/${PKGNAME}.pkg complete!"
	@echo
