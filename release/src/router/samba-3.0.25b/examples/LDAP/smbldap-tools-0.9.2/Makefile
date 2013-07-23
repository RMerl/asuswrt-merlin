PACKAGE=smbldap-tools
VERSION=0.9.2
RELEASE=1
DESTARCH=smbldap-tools-$(VERSION)
#RELEASE=$(shell date +%s)

# where to build the rpm
TOPDIR=/home/jtournier/redhat
GPG_PATH=/home/jtournier/.gnupg
BUILD_CMD=rpmbuild -ba
#BUILD_CMD=rpmbuild -ba --sign

prefix=/opt/IDEALX
sbindir=$(prefix)/sbin
sysconfdir=/etc/opt/IDEALX
make=/usr/bin/make
install=/usr/bin/install
rm=/bin/rm
sed=/bin/sed

prep:
	$(sed) -e 's|@SBINDIR@|$(sbindir)|g' smb.conf.in > smb.conf

all:	prep distclean rpm

install:
	@mkdir -p $(sbindir)
	@mkdir -p $(sysconfdir)/smbldap-tools/
	$(install) -m0755 smbldap-* smbldap_tools.pm $(sbindir)
	$(install) -m0644 smbldap.conf $(sysconfdir)/smbldap-tools/
	$(install) -m0600 smbldap_bind.conf $(sysconfdir)/smbldap-tools/

clean:
	$(rm) -f smb.conf smbldap.conf

distclean:
	rm -f *~

dist: .diststamp
	@if [ -d $(DESTARCH) ];then echo "About to remove ./$(DESTARCH).tgz in 5 seconds ..."; sleep 5; fi
	@mkdir -p $(DESTARCH)/doc
	@cp smbldap-tools.spec $(DESTARCH)
	@perl -i -pe's@^\%define version(.*)@\%define version $(VERSION)@' $(DESTARCH)/smbldap-tools.spec
	@perl -i -pe's@^\%define release(.*)@\%define release $(RELEASE)@' $(DESTARCH)/smbldap-tools.spec
	@cp Makefile $(DESTARCH)
	@cp CONTRIBUTORS $(DESTARCH)
	@cp COPYING $(DESTARCH)
	@cp ChangeLog $(DESTARCH)
	@cp FILES $(DESTARCH)
	@cp INSTALL $(DESTARCH)
	@cp README $(DESTARCH)
	@cp TODO $(DESTARCH)
	@cp INFRA $(DESTARCH)
	@cp smbldap-populate $(DESTARCH)
	@cp smbldap-groupadd $(DESTARCH)
	@cp smbldap-groupshow $(DESTARCH)
	@cp smbldap-groupmod $(DESTARCH)
	@cp smbldap-groupdel $(DESTARCH)
	@cp smbldap-useradd $(DESTARCH)
	@cp smbldap-usershow $(DESTARCH)
	@cp smbldap-usermod $(DESTARCH)
	@cp smbldap-userinfo $(DESTARCH)
	@cp smbldap-userdel $(DESTARCH)
	@cp smbldap-passwd $(DESTARCH)
	@cp smbldap_bind.conf $(DESTARCH)
	@cp smbldap_tools.pm $(DESTARCH)
	@cp smbldap.conf $(DESTARCH)
	@cp smb.conf $(DESTARCH)
	@cp configure.pl $(DESTARCH)
	@cp smbldap-migrate-* $(DESTARCH)/doc/
	@cp -r doc/*.pdf doc/html $(DESTARCH)/doc/
	echo "Creating tarball...";
	@tar czf $(DESTARCH).tgz $(DESTARCH)
	@rm -r $(DESTARCH)
	@touch .diststamp

build_dir:
	@echo '%_topdir $(TOPDIR)' > $(HOME)/.rpmmacros
	@echo '%_signature gpg' >> $(HOME)/.rpmmacros
	@echo '%_gpg_name Jérôme Tournier <jtournier@idealx.com>' >> $(HOME)/.rpmmacros
	@echo '%_gpg_path $(GPG_PATH)' >> $(HOME)/.rpmmacros
	@mkdir -p $(TOPDIR)/BUILD
	@mkdir -p $(TOPDIR)/RPMS/i386
	@mkdir -p $(TOPDIR)/SOURCES
	@mkdir -p $(TOPDIR)/SPECS
	@mkdir -p $(TOPDIR)/SRPMS

rpm: dist build_dir
	@cp -f $(DESTARCH).tgz $(TOPDIR)/SOURCES/
	@cp -f smbldap-tools.spec $(TOPDIR)/SPECS/
	@perl -i -pe's@^\%define version(.*)@\%define version $(VERSION)@' $(TOPDIR)/SPECS/smbldap-tools.spec
	@perl -i -pe's@^\%define release(.*)@\%define release $(RELEASE)@' $(TOPDIR)/SPECS/smbldap-tools.spec
	@cd $(TOPDIR)/SPECS/ && $(BUILD_CMD) smbldap-tools.spec


