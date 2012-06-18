PACKAGE=LPRng
VERSION=3.8.22
RELEASE=1
RPM=/usr/src/redhat/RPMS/i386/${PACKAGE}-${VERSION}-${RELEASE}.i386.rpm
CONFIGURE_ARGS=--prefix=/usr --sysconfdir=/etc --with-filterdir=/usr/libexec/filters --with-ld_library_path=/lib:/usr/lib:/usr/local/lib --with-filter_path=/bin:/usr/bin:/usr/libexec:/usr/local/libexec:/usr/sbin:/var/spool/bin

COPY_HERE=
.PHONY: all rpm testi testu dou doU undo update clean checkit

all:
	make update
	chown root.bin *
	make rpm

rpm:	${RPM}

testi:
	rpm -i --test ${RPM}
testu:
	rpm -U --test ${RPM} || echo $$?
dou:
	rpm -U  ${RPM} || echo ERROR $$?
doU:
	rpm -U --force ${RPM} || echo ERROR $$?
undo:
	rpm -e --nodeps ${PACKAGE}

${RPM}: ${PACKAGE}.spec
	cp lpd.init /usr/src/redhat/SOURCES/lpd.init
	rpmbuild -ba ${PACKAGE}.spec

clean:
	rm -f *.tgz
	rm -rf /usr/src/redhat/BUILD/${PACKAGE}-*

update:
	perl -spi -e "\
		s|^Version: .*|Version: ${VERSION}|; \
		s|^Release: .*|Release: ${RELEASE}|; \
		" ${PACKAGE}.spec;

checkit: update
	chown root.bin *
	make rpm
