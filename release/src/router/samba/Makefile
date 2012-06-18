include ../common.mak

srcdir=./source
idir=

all: config
	$(MAKE) -C source all shared LD="$(CC) -shared -Wl,-Bsymbolic"

config: $(srcdir)/Makefile

$(srcdir)/Makefile: 
	cd $(srcdir) && \
		ac_cv_sizeof_int=4 ac_cv_sizeof_long=4 ac_cv_sizeof_short=2 \
		samba_cv_FTRUNCATE_NEEDS_ROOT=no samba_cv_HAVE_BROKEN_FCNTL64_LOCKS=no \
		samba_cv_HAVE_BROKEN_GETGROUPS=no samba_cv_HAVE_BROKEN_READDIR=no \
		samba_cv_HAVE_FCNTL_LOCK=yes samba_cv_HAVE_FNMATCH=yes \
		samba_cv_HAVE_FTRUNCATE_EXTEND=yes samba_cv_HAVE_IFACE_AIX=no \
		samba_cv_HAVE_IFACE_IFCONF=yes samba_cv_HAVE_IFACE_IFREQ=yes \
		samba_cv_HAVE_INO64_T=yes samba_cv_HAVE_IRIX_SPECIFIC_CAPABILITIES=no \
		samba_cv_HAVE_OFF64_T=yes samba_cv_HAVE_ROOT=yes \
		samba_cv_HAVE_SECURE_MKSTEMP=yes samba_cv_HAVE_SHARED_MMAP=yes \
		samba_cv_HAVE_STRUCT_FLOCK64=yes samba_cv_HAVE_SYSV_IPC=no \
		samba_cv_HAVE_TRUNCATED_SALT=no samba_cv_HAVE_UNION_SEMUN=no \
		samba_cv_HAVE_UNSIGNED_CHAR=yes samba_cv_NEED_SGI_SEMUN_HACK=no \
		samba_cv_REPLACE_INET_NTOA=no samba_cv_SIZEOF_INO_T=4 \
		samba_cv_SIZEOF_OFF_T=4 samba_cv_SYSCONF_SC_NGROUPS_MAX=yes \
		samba_cv_USE_SETRESUID=no samba_cv_USE_SETREUID=yes \
		samba_cv_USE_SETEUID=yes samba_cv_USE_SETUIDX=no \
		samba_cv_have_longlong=yes samba_cv_have_setresgid=no \
		samba_cv_have_setresuid=no samba_cv_HAVE_GETTIMEOFDAY_TZ=yes \
		CPPFLAGS="-D_GNU_SOURCE -DNDEBUG -DSHMEM_SIZE=524288 -Dfcntl=fcntl64" \
		CFLAGS="-Os $(EXTRACFLAGS) -ffunction-sections -fdata-sections" \
		LDFLAGS="$(CFLAGS) -Wl,--gc-sections" \
		CC=$(CC) LD=$(LD) AR=$(AR) RANLIB=$(RANLIB) ./configure --host=mipsel-linux \
		--prefix=/usr --localstatedir=/var/log --libdir=/etc --sysconfdir=/etc/samba \
		--with-privatedir=/etc/samba --with-lockdir=/var/run/samba --with-syslog

install: all
	install -D $(srcdir)/bin/smbd.shared $(INSTALLDIR)$(idir)/usr/sbin/smbd
	install -D $(srcdir)/bin/nmbd.shared $(INSTALLDIR)$(idir)/usr/sbin/nmbd
	install -D $(srcdir)/bin/smbpasswd.shared $(INSTALLDIR)$(idir)/usr/bin/smbpasswd
	install -D $(srcdir)/bin/libsmb.so $(INSTALLDIR)$(idir)/usr/lib/libsmb.so
	install -d $(INSTALLDIR)$(idir)/usr/codepages/
	install -D $(srcdir)/codepages/codepage.* $(INSTALLDIR)$(idir)/usr/codepages
	install -D $(srcdir)/codepages/unicode_map.* $(INSTALLDIR)$(idir)/usr/codepages
ifneq ($(CONFIG_LINUX26),y)
	rm -f $(INSTALLDIR)$(idir)/usr/codepages/*.932
	rm -f $(INSTALLDIR)$(idir)/usr/codepages/*.936
	rm -f $(INSTALLDIR)$(idir)/usr/codepages/*.949
	rm -f $(INSTALLDIR)$(idir)/usr/codepages/*.950
endif
	$(STRIP) -s $(INSTALLDIR)$(idir)/usr/sbin/smbd
	$(STRIP) -s $(INSTALLDIR)$(idir)/usr/sbin/nmbd
	$(STRIP) -s $(INSTALLDIR)$(idir)/usr/bin/smbpasswd
#	$(STRIP) $(INSTALLDIR)$(idir)/usr/lib/libsmb.so

clean:
	@rm -f source/bin/*
	-$(MAKE) -C source clean
	@rm -rf $(srcdir)/Makefile
	@rm -rf config.{cache,status}
