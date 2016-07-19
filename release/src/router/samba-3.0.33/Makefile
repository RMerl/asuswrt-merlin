include ../common.mak

srcdir=source

SMBCFLAGS = $(EXTRACFLAGS) -Os -ffunction-sections -fdata-sections
SMBLDFLAGS = -ffunction-sections -fdata-sections -Wl,--gc-sections

ifneq ($(CONFIG_LINUX26),y)
SMBCFLAGS += -DMAX_DEBUG_LEVEL="-1"
endif

ifeq ($(RTCONFIG_RECVFILE), y)
SMBCFLAGS += -DHAVE_BCM_RECVFILE
endif

ifeq ($(RTCONFIG_BCMARM), y)
SMBLDFLAGS += -lgcc_s
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
	export samba_cv_have_longlong=yes ; \
	ac_cv_lib_resolv___dn_expand=no \
	ac_cv_func_prctl=no \
	samba_cv_HAVE_INO64_T=yes \
	samba_cv_HAVE_OFF64_T=yes \
	samba_cv_HAVE_STRUCT_FLOCK64=yes \
	samba_cv_SIZEOF_OFF_T=yes \
	samba_cv_HAVE_MMAP=yes \
	samba_cv_HAVE_FTRUNCATE_EXTEND=yes \
	samba_cv_HAVE_BROKEN_LINUX_SENDFILE=no \
	samba_cv_have_setresgid=yes \
	samba_cv_have_setresuid=yes \
	samba_cv_USE_SETREUID=yes \
	samba_cv_REALPATH_TAKES_NULL=no \
	samba_cv_HAVE_FCNTL_LOCK=yes \
	samba_cv_HAVE_SECURE_MKSTEMP=yes \
	samba_cv_HAVE_NATIVE_ICONV=no \
	samba_cv_HAVE_BROKEN_FCNTL64_LOCKS=no \
	samba_cv_HAVE_BROKEN_GETGROUPS=no \
	samba_cv_HAVE_BROKEN_READDIR_NAME=no \
	samba_cv_HAVE_C99_VSNPRINTF=yes \
	samba_cv_HAVE_DEV64_T=no \
	samba_cv_HAVE_DEVICE_MAJOR_FN=yes \
	samba_cv_HAVE_DEVICE_MINOR_FN=yes \
	samba_cv_HAVE_IFACE_AIX=no \
	samba_cv_HAVE_KERNEL_CHANGE_NOTIFY=yes \
	samba_cv_HAVE_MAKEDEV=yes \
	samba_cv_HAVE_TRUNCATED_SALT=no \
	samba_cv_HAVE_UNSIGNED_CHAR=no \
	samba_cv_HAVE_WORKING_AF_LOCAL=yes \
	samba_cv_HAVE_Werror=yes \
	samba_cv_REPLACE_INET_NTOA=no \
	samba_cv_SIZEOF_DEV_T=yes \
	samba_cv_SIZEOF_INO_T=yes \
	samba_cv_SIZEOF_TIME_T=no \
	CPPFLAGS="-DNDEBUG -DSHMEM_SIZE=524288 -Dfcntl=fcntl64 -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE=1 -D_LARGEFILE64_SOURCE=1 -D_LARGE_FILES=1" \
	CFLAGS="$(SMBCFLAGS)" LDFLAGS="$(SMBLDFLAGS)" CC=$(CC) LD=$(LD) AR=$(AR) RANLIB=$(RANLIB) \
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
	#@install -D $(srcdir)/bin/libsmbclient.so $(INSTALLDIR)/usr/lib/libsmbclient.so
	#@install -D $(srcdir)/bin/libbigballofmud.so.0 $(INSTALLDIR)/usr/lib/libbigballofmud.so.0
	@install -D $(shell dirname $(shell which $(CXX)))/../lib/libresolv.so.0 $(INSTALLDIR)/usr/lib/libresolv.so.0
###############################Charles Modify##########	

	$(STRIP) -s $(INSTALLDIR)/usr/sbin/smbd
	$(STRIP) -s $(INSTALLDIR)/usr/sbin/nmbd
	$(STRIP) -s $(INSTALLDIR)/usr/bin/smbpasswd
#	$(STRIP) -s $(INSTALLDIR)/usr/lib/libsmbclient.so
	# do not strip shared library, it will be optimized by libfoo.pl
	# $(STRIP) -s $(INSTALLDIR)/usr/lib/libbigballofmud.so
