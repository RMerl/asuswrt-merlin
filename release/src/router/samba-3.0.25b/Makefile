include ../common.mak

#srcdir=source
srcdir=source

SMBCFLAGS = $(EXTRACFLAGS) -Os -ffunction-sections -fdata-sections
SMBLDFLAGS = -ffunction-sections -fdata-sections -Wl,--gc-sections

ifeq ($(RTCONFIG_BCMARM), y)
SMBLDFLAGS += -lgcc_s
endif

ifneq ($(CONFIG_LINUX26),y)
SMBCFLAGS += -DMAX_DEBUG_LEVEL="-1"
endif

ifeq ($(RTCONFIG_BCMARM), y)
HOST = arm
else
HOST = mips
endif

all: .conf apps

apps: .conf
	mkdir -p $(srcdir)/bin
	$(MAKE) -C $(srcdir) all

.conf:
	cd $(srcdir) && \
	 export SMB_BUILD_CC_NEGATIVE_ENUM_VALUES=yes ; \
	 export libreplace_cv_READDIR_GETDIRENTRIES=no ; \
	 export libreplace_cv_READDIR_GETDENTS=no ; \
	 export linux_getgrouplist_ok=no ; \
	 export samba_cv_REPLACE_READDIR=no ; \
	 export samba_cv_HAVE_WRFILE_KEYTAB=yes ; \
	 export samba_cv_HAVE_KERNEL_OPLOCKS_LINUX=yes ; \
	 export samba_cv_HAVE_KERNEL_CHANGE_NOTIF=yes ; \
	 export samba_cv_HAVE_KERNEL_SHARE_MODES=yes ; \
	 export samba_cv_HAVE_IFACE_IFCONF=yes ; \
	 export samba_cv_USE_SETRESUID=yes ; \
	$(CONFIGURE) \
		--prefix=/usr \
		--bindir=/usr/bin \
		--sbindir=/usr/sbin \
		--libdir=/etc \
		--localstatedir=/var \
		--host=$(HOST)-linux \
		--with-configdir=/etc \
		--with-rootsbindir=/usr/sbin \
		--with-piddir=/var/run/samba \
		--with-privatedir=/etc/samba \
		--with-lockdir=/var/lock \
		--with-included-popt=no \
		--with-krb5=no \
		--with-shared-modules=MODULES \
		--enable-shared \
		--disable-static \
		--disable-cups \
		--disable-iprint \
		--disable-pie \
		--disable-fam \
		--disable-dmalloc \
		--disable-krb5developer \
		--disable-developer \
		--disable-debug \
		--without-ads \
		--without-acl-support \
		--without-ldap \
		--without-cifsmount \
		--without-cifsupcall \
		--without-cluster-support \
		--without-utmp \
		--without-winbind \
		--without-quotas \
		--without-sys-quotas
	touch .conf
	mkdir -p $(srcdir)/bin

clean:
	-$(MAKE) -C $(srcdir) distclean
	@rm -f .conf

distclean: clean
	@find $(srcdir) -name config.h | xargs rm -f
	@find $(srcdir) -name Makefile | xargs rm -f
	@find $(srcdir) -name config.status | xargs rm -f
	@find $(srcdir) -name config.cache | xargs rm -f

install: all
	@install -d $(INSTALLDIR)/usr/bin/
	@install -d $(INSTALLDIR)/usr/sbin/
	@install -d $(INSTALLDIR)/usr/lib/
	@install -D $(srcdir)/bin/smbd $(INSTALLDIR)/usr/sbin/smbd
	@install -D $(srcdir)/bin/nmbd $(INSTALLDIR)/usr/sbin/nmbd
	@install -D $(srcdir)/bin/smbpasswd $(INSTALLDIR)/usr/bin/smbpasswd
	# @install -D $(srcdir)/bin/libsmbclient.so $(INSTALLDIR)/usr/lib/libsmbclient.so
	@install -D $(srcdir)/bin/libbigballofmud.so.0 $(INSTALLDIR)/usr/lib/libbigballofmud.so.0
	@cp prebuilt/lib/libresolv.so.0 $(INSTALLDIR)/usr/lib/libresolv.so.0
###############################Charles Modify##########	

	$(STRIP) -s $(INSTALLDIR)/usr/sbin/smbd
	$(STRIP) -s $(INSTALLDIR)/usr/sbin/nmbd
	$(STRIP) -s $(INSTALLDIR)/usr/bin/smbpasswd
#	$(STRIP) -s $(INSTALLDIR)/usr/lib/libsmbclient.so
	# do not strip shared library, it will be optimized by libfoo.pl
	# $(STRIP) -s $(INSTALLDIR)/usr/lib/libbigballofmud.so
