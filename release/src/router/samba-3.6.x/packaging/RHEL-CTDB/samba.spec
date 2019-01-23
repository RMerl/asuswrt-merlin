%define initdir %{_sysconfdir}/rc.d/init.d
%define auth %(test -f /etc/pam.d/system-auth && echo /etc/pam.d/system-auth || echo)

Summary: Samba SMB client and server
Vendor: Samba Team
Packager: Samba Team <samba@samba.org>
Name:         samba
Version:      3.6.25
Release:      1GITHASH
Epoch:        0
License: GNU GPL version 3
Group: System Environment/Daemons
URL: http://www.samba.org/

Source: samba-%{version}.tar.bz2

# Don't depend on Net::LDAP
Source998: filter-requires-samba.sh
Source999: setup.tar.bz2

Requires: /sbin/chkconfig /bin/mktemp /usr/bin/killall
Requires: fileutils sed /etc/init.d

Requires: pam >= 0.64 %{auth} 
Requires: samba-common = %{version}-%{release}
Provides: samba = %{version}

Prefix: /usr
BuildRoot: %{_tmppath}/%{name}-%{version}-root
BuildRequires: pam-devel, readline-devel, fileutils, libacl-devel, openldap-devel, krb5-devel, cups-devel, e2fsprogs-devel
# requirements for building the man pages:
BuildRequires: libxslt, docbook-utils, docbook-style-xsl
BuildRequires: ctdb-devel >= 1.2.25

# Working around perl dependency problem from docs
%define __perl_requires %{SOURCE998}

# rpm screws up the arch lib dir when using --target on RHEL5
%ifarch i386 i486 i586 i686 ppc s390
%define _libarch lib
%else
%define _libarch %_lib
%endif

%define _libarchdir /usr/%{_libarch}

%define numcpu  %(grep "^processor" /proc/cpuinfo |wc -l | sed -e 's/^0$/1/')

%description
Samba is the protocol by which a lot of PC-related machines share
files, printers, and other information (such as lists of available
files and printers). The Windows NT, OS/2, and Linux operating systems
support this natively, and add-on packages can enable the same thing
for DOS, Windows, VMS, UNIX of all kinds, MVS, and more. This package
provides an SMB server that can be used to provide network services to
SMB (sometimes called "Lan Manager") clients. Samba uses NetBIOS over
TCP/IP (NetBT) protocols and does NOT need the NetBEUI (Microsoft Raw
NetBIOS frame) protocol.


######################################################################
%package client
Summary: Samba (SMB) client programs.
Group: Applications/System
Requires: samba-common = %{version}-%{release}
Obsoletes: smbfs
Provides: samba-client = %{version}-%{release}

%description client
The samba-client package provides some SMB clients to compliment the
built-in SMB filesystem in Linux. These clients allow access of SMB
shares and printing to SMB printers.


#######################################################################
%package common
Summary: Files used by both Samba servers and clients.
Group: Applications/System
Provides: samba-common = %{version}-%{release}

%description common
Samba-common provides files necessary for both the server and client
packages of Samba.


#######################################################################
%package swat
Summary: The Samba SMB server configuration program.
Group: Applications/System
Requires: samba = %{version} xinetd
Provides: samba-swat = %{version}-%{release}

%description swat
The samba-swat package includes the new SWAT (Samba Web Administration
Tool), for remotely managing Samba's smb.conf file using your favorite
Web browser.

%ifarch x86_64 ppc64
%package winbind-32bit
Summary:        Samba winbind compatibility package for 32bit apps on 64bit archs
Group:          Applications/System

%description winbind-32bit
Compatibility package for 32 bit apps on 64 bit architecures
%endif


#######################################################################
%package doc
Summary:      Samba Documentation
Group:        Documentation/Other
Provides:     samba-doc = %{version}-%{release}
Requires:       /usr/bin/find /bin/rm /usr/bin/xargs

%description doc
The samba-doc package includes the HTML versions of the Samba manpages
utilized by SWAT as well as the HTML and PDF version of "Using Samba",
"Samba By Example", and "The Official Samba HOWTO and Reference Guide".


#######################################################################

%prep
%setup -q

# setup the vendor files (init scripts, etc...)
%setup -T -D -a 999 -n samba-%{version} -q

%build

/bin/cp setup/filter-requires-samba.sh %{SOURCE998}

cd source3
# RPM_OPT_FLAGS="$RPM_OPT_FLAGS -D_FILE_OFFSET_BITS=64"

## check for ccache
if ccache -h >/dev/null 2>&1 ; then
	CC="ccache gcc"
else
	CC="gcc"
fi

export CC

## always run autogen.sh
./autogen.sh


##
## build the files for the winbind-32bit compat package
## and copy them to a safe location
##
%ifarch x86_64 ppc64

# a directory to store the 32bit compatibility modules for later install
%define _32bit_tmp_dir %{_tmppath}/%{name}-%{version}-32bit

CC_SAVE="$CC"
CC="$CC -m32"

CFLAGS="$RPM_OPT_FLAGS -O3 -D_GNU_SOURCE -m32" ./configure \
	--prefix=%{_prefix} \
	--localstatedir=/var \
        --with-configdir=%{_sysconfdir}/samba \
        --with-libdir=/usr/lib/samba \
	--with-pammodulesdir=/lib/security \
        --with-lockdir=/var/lib/samba \
        --with-logfilebase=/var/log/samba \
        --with-mandir=%{_mandir} \
        --with-piddir=/var/run \
	--with-privatedir=%{_sysconfdir}/samba \
	--disable-cups \
        --with-acl-support \
	--with-ads \
        --with-automount \
        --with-fhs \
	--with-pam_smbpass \
	--with-libsmbclient \
	--with-libsmbsharemodes \
        --without-smbwrapper \
	--with-pam \
	--with-quotas \
	--with-syslog \
	--with-utmp \
	--with-cluster-support \
	--with-ctdb=/usr/include \
	--without-ldb \
	--without-dnsupdate \
	--with-aio-support \
	--disable-merged-build

make showlayout

make samba3-idl

## check for gcc 3.4 or later
CC_VERSION=`${CC} --version | head -1 | awk '{print $3}'`
CC_MAJOR=`echo ${CC_VERSION} | cut -d. -f 1`
CC_MINOR=`echo ${CC_VERSION} | cut -d. -f 2`
if [ ${CC_MAJOR} -ge 3 ]; then
        if [ ${CC_MAJOR} -gt 3 -o ${CC_MINOR} -ge 4 ]; then
                make pch
        fi
fi

make -j%{numcpu} %{?_smp_mflags} \
	nss_modules pam_modules

rm -rf %{_32bit_tmp_dir}
mkdir %{_32bit_tmp_dir}

mv ../nsswitch/libnss_winbind.so %{_32bit_tmp_dir}/
mv bin/pam_winbind.so %{_32bit_tmp_dir}/
mv bin/libtalloc.so* %{_32bit_tmp_dir}/
mv bin/libtdb.so* %{_32bit_tmp_dir}/
mv bin/libwbclient.so* %{_32bit_tmp_dir}/

make clean

CC="$CC_SAVE"

%endif

CFLAGS="$RPM_OPT_FLAGS $EXTRA -D_GNU_SOURCE" ./configure \
	--prefix=%{_prefix} \
	--localstatedir=/var \
        --with-configdir=%{_sysconfdir}/samba \
        --libdir=%{_libarchdir} \
	--with-modulesdir=%{_libarchdir}/samba \
	--with-pammodulesdir=%{_libarch}/security \
        --with-lockdir=/var/lib/samba \
        --with-logfilebase=/var/log/samba \
        --with-mandir=%{_mandir} \
        --with-piddir=/var/run \
	--with-privatedir=%{_sysconfdir}/samba \
        --with-sambabook=%{_datadir}/swat/using_samba \
        --with-swatdir=%{_datadir}/swat \
	--disable-cups \
        --with-acl-support \
	--with-ads \
        --with-automount \
        --with-fhs \
	--with-pam_smbpass \
	--with-libsmbclient \
	--with-libsmbsharemodes \
        --without-smbwrapper \
	--with-pam \
	--with-quotas \
	--with-shared-modules=idmap_rid,idmap_ad,idmap_tdb2,vfs_gpfs,vfs_tsmsm,vfs_gpfs_hsm_notify \
	--with-syslog \
	--with-utmp \
	--with-cluster-support \
	--with-ctdb=/usr/include \
	--without-ldb \
	--without-dnsupdate \
	--with-aio-support\
	--disable-merged-build

make showlayout

## check for gcc 3.4 or later
CC_VERSION=`${CC} --version | head -1 | awk '{print $3}'`
CC_MAJOR=`echo ${CC_VERSION} | cut -d. -f 1`
CC_MINOR=`echo ${CC_VERSION} | cut -d. -f 2`
if [ ${CC_MAJOR} -ge 3 ]; then
        if [ ${CC_MAJOR} -gt 3 -o ${CC_MINOR} -ge 4 ]; then
                make pch
        fi
fi


make -j %{numcpu} %{?_smp_mflags} \
	everything modules pam_smbpass

# check that desired suppor has been compiled into smbd:
export LD_LIBRARY_PATH=./bin
for test in HAVE_POSIX_ACLS HAVE_LDAP HAVE_KRB5 HAVE_GPFS CLUSTER_SUPPORT
do
	if ! $(./bin/smbd -b | grep -q $test ) ; then
		echo "ERROR: '$test' is not in smbd. Build stopped."
		exit 1;
	fi
done

# try and build the manpages
cd ..
./release-scripts/build-manpages-nogit

# Remove some permission bits to avoid to many dependencies
find examples docs -type f | xargs -r chmod -x

%install
# Clean up in case there is trash left from a previous build
rm -rf $RPM_BUILD_ROOT

# Create the target build directory hierarchy
mkdir -p $RPM_BUILD_ROOT%{_datadir}/swat/{help,include,using_samba/{figs,gifsa}}
mkdir -p $RPM_BUILD_ROOT%{_includedir}
mkdir -p $RPM_BUILD_ROOT%{_initrddir}
mkdir -p $RPM_BUILD_ROOT{%{_libarchdir},%{_includedir}}
mkdir -p $RPM_BUILD_ROOT%{_libarchdir}/samba/{auth,charset,idmap,vfs,pdb}
mkdir -p $RPM_BUILD_ROOT/%{_libarch}/security
mkdir -p $RPM_BUILD_ROOT/lib/security
mkdir -p $RPM_BUILD_ROOT%{_mandir}
mkdir -p $RPM_BUILD_ROOT%{_prefix}/{bin,sbin}
mkdir -p $RPM_BUILD_ROOT%{_prefix}/lib
mkdir -p $RPM_BUILD_ROOT/sbin
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/{pam.d,samba}
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/{pam.d}
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/rc.d/init.d
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/{samba,sysconfig}
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/xinetd.d
mkdir -p $RPM_BUILD_ROOT/var/lib/samba/winbindd_privileged
mkdir -p $RPM_BUILD_ROOT/var/{log,run/winbindd,spool}/samba
mkdir -p $RPM_BUILD_ROOT/%{_libarchdir}/krb5/plugins/libkrb5

cd source3
make DESTDIR=$RPM_BUILD_ROOT \
        install

make DESTDIR=$RPM_BUILD_ROOT \
        install-dbwrap_tool install-dbwrap_torture
cd ..

# NSS winbind support
install -m 755 nsswitch/libnss_winbind.so $RPM_BUILD_ROOT/%{_libarch}/libnss_winbind.so.2
( cd $RPM_BUILD_ROOT/%{_libarch};
  ln -sf libnss_winbind.so.2  libnss_winbind.so )
#
# do not install libnss_wins.so in order to reduce dependencies
# (we do not need it for the samba-ctdb scenario)
#
#install -m 755 nsswitch/libnss_wins.so $RPM_BUILD_ROOT/%{_libarch}/libnss_wins.so
# ( cd $RPM_BUILD_ROOT/%{_libarch}; ln -sf libnss_wins.so  libnss_wins.so.2 )

cp -p source3/bin/winbind_krb5_locator.so ${RPM_BUILD_ROOT}/%{_libarchdir}/krb5/plugins/libkrb5

# install files for winbind-32bit package
%ifarch x86_64 ppc64

install -m 755 %{_32bit_tmp_dir}/libnss_winbind.so ${RPM_BUILD_ROOT}/lib/libnss_winbind.so.2
( cd ${RPM_BUILD_ROOT}/lib; ln -sf libnss_winbind.so.2  libnss_winbind.so )

mv %{_32bit_tmp_dir}/libtalloc* ${RPM_BUILD_ROOT}/usr/lib
mv %{_32bit_tmp_dir}/libtdb* ${RPM_BUILD_ROOT}/usr/lib
mv %{_32bit_tmp_dir}/libwbclient* ${RPM_BUILD_ROOT}/usr/lib
mv %{_32bit_tmp_dir}/pam_winbind.so ${RPM_BUILD_ROOT}/lib/security

rm -rf %{_32bit_tmp_dir}

%endif

## cleanup
/bin/rm -rf $RPM_BUILD_ROOT/usr/lib*/samba/security

# Install the miscellany
echo 127.0.0.1 localhost > $RPM_BUILD_ROOT%{_sysconfdir}/samba/lmhosts

install -m644 setup/swat $RPM_BUILD_ROOT%{_sysconfdir}/xinetd.d/swat
install -m644 setup/samba.sysconfig $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig/samba
install -m755 setup/smb.init $RPM_BUILD_ROOT%{initdir}/smb
install -m755 setup/winbind.init $RPM_BUILD_ROOT%{initdir}/winbind
install -m644 setup/samba.pamd $RPM_BUILD_ROOT%{_sysconfdir}/pam.d/samba
install -m755 setup/smbprint $RPM_BUILD_ROOT%{_bindir}
install -m644 setup/smbusers $RPM_BUILD_ROOT%{_sysconfdir}/samba/smbusers
install -m644 setup/smb.conf $RPM_BUILD_ROOT%{_sysconfdir}/samba/smb.conf
install -m755 source3/script/mksmbpasswd.sh $RPM_BUILD_ROOT%{_bindir}

ln -s ../..%{initdir}/smb  $RPM_BUILD_ROOT%{_sbindir}/samba
ln -s ../..%{initdir}/winbind  $RPM_BUILD_ROOT%{_sbindir}/winbind

# Remove "*.old" files
find $RPM_BUILD_ROOT -name "*.old" -exec rm -f {} \;


##
## Clean out man pages for tools not installed here
##
rm -f $RPM_BUILD_ROOT%{_mandir}/man1/log2pcap.1*
rm -f $RPM_BUILD_ROOT%{_mandir}/man1/smbsh.1*
rm -f $RPM_BUILD_ROOT%{_mandir}/man5/vfstest.1*


%clean
rm -rf $RPM_BUILD_ROOT

%post
## deal with an upgrade from a broken 3.0.21b RPM
if [ "$1" -eq "2" ]; then
	if [ -d /var/cache/samba ]; then
		for file in `ls /var/cache/samba/*tdb`; do
			/bin/cp -up $file /var/lib/samba/`basename $file`
		done
		mkdir -p /var/lib/samba/eventlog
		for file in `ls /var/cache/samba/eventlog/*tdb`; do
			/bin/cp -up $file /var/lib/samba/eventlog/`basename $file`
		done
		/bin/mv /var/cache/samba /var/cache/samba.moved
        fi
fi

if [ "$1" -ge "1" ]; then
	/sbin/service smb condrestart >/dev/null 2>&1 || :
fi

%preun
if [ $1 = 0 ] ; then
    /sbin/service smb stop >/dev/null 2>&1 || :
    /sbin/chkconfig --del smb
    # rm -rf /var/log/samba/* /var/lib/samba/*
fi
exit 0

#%postun

%post swat
# Add swat entry to /etc/services if not already there.
if [ ! "`grep ^\s**swat /etc/services`" ]; then
        echo 'swat        901/tcp     # Add swat service used via inetd' >> /etc/services
fi

%post common
/sbin/ldconfig

if [ "$1" -ge "1" ]; then
	/sbin/service winbind condrestart >/dev/null 2>&1 || :
fi

%preun common
if [ $1 = 0 ] ; then
    /sbin/service winbind stop >/dev/null 2>&1 || :
    /sbin/chkconfig --del winbind
fi
exit 0

%postun common 
/sbin/ldconfig

#######################################################################
## Files section                                                     ##
#######################################################################

%files
%defattr(-,root,root)

%config(noreplace) %{_sysconfdir}/sysconfig/samba
%config(noreplace) %{_sysconfdir}/samba/smbusers
%attr(755,root,root) %config %{initdir}/smb
%config(noreplace) %{_sysconfdir}/pam.d/samba

%attr(0755,root,root) %dir /var/log/samba
%attr(0755,root,root) %dir /var/lib/samba
%attr(1777,root,root) %dir /var/spool/samba

%{_sbindir}/samba

%{_sbindir}/smbd
%{_sbindir}/nmbd

%{_bindir}/mksmbpasswd.sh
%{_bindir}/smbcontrol
%{_bindir}/smbstatus
%{_bindir}/tdbbackup
%{_bindir}/tdbtool
%{_bindir}/tdbdump
%{_bindir}/tdbrestore
%{_bindir}/eventlogadm

%{_libarchdir}/samba/auth/script.so
%{_libarchdir}/samba/vfs/acl_tdb.so
%{_libarchdir}/samba/vfs/acl_xattr.so
%{_libarchdir}/samba/vfs/aio_fork.so
%{_libarchdir}/samba/vfs/audit.so
%{_libarchdir}/samba/vfs/cap.so
%{_libarchdir}/samba/vfs/catia.so
%{_libarchdir}/samba/vfs/crossrename.so
%{_libarchdir}/samba/vfs/default_quota.so
%{_libarchdir}/samba/vfs/dirsort.so
%{_libarchdir}/samba/vfs/expand_msdfs.so
%{_libarchdir}/samba/vfs/extd_audit.so
%{_libarchdir}/samba/vfs/fake_perms.so
%{_libarchdir}/samba/vfs/fileid.so
%{_libarchdir}/samba/vfs/full_audit.so
%{_libarchdir}/samba/vfs/gpfs.so
%{_libarchdir}/samba/vfs/gpfs_hsm_notify.so
%{_libarchdir}/samba/vfs/linux_xfs_sgid.so
%{_libarchdir}/samba/vfs/netatalk.so
%{_libarchdir}/samba/vfs/preopen.so
%{_libarchdir}/samba/vfs/readahead.so
%{_libarchdir}/samba/vfs/readonly.so
%{_libarchdir}/samba/vfs/recycle.so
%{_libarchdir}/samba/vfs/scannedonly.so
%{_libarchdir}/samba/vfs/shadow_copy.so
%{_libarchdir}/samba/vfs/shadow_copy2.so
%{_libarchdir}/samba/vfs/smb_traffic_analyzer.so
%{_libarchdir}/samba/vfs/streams_depot.so
%{_libarchdir}/samba/vfs/streams_xattr.so
%{_libarchdir}/samba/vfs/syncops.so
%{_libarchdir}/samba/vfs/time_audit.so
%{_libarchdir}/samba/vfs/tsmsm.so
%{_libarchdir}/samba/vfs/xattr_tdb.so


%{_mandir}/man1/smbcontrol.1*
%{_mandir}/man1/smbstatus.1*
%{_mandir}/man1/vfstest.1*
%{_mandir}/man5/smbpasswd.5*
%{_mandir}/man5/pam_winbind.conf.5*
%{_mandir}/man7/samba.7*
%{_mandir}/man8/nmbd.8*
%{_mandir}/man8/pdbedit.8*
%{_mandir}/man8/smbd.8*
%{_mandir}/man8/tdbbackup.8*
%{_mandir}/man8/tdbdump.8*
%{_mandir}/man8/tdbtool.8*
%{_mandir}/man8/eventlogadm.8*
%{_mandir}/man8/vfs_*.8*
%{_mandir}/man8/smbta-util.8*


##########

%files doc
%defattr(-,root,root)
%doc README
%doc COPYING
%doc Manifest 
%doc WHATSNEW.txt
%doc Roadmap
%doc docs-xml/archives/THANKS
%doc docs-xml/archives/history
%doc docs-xml/registry
%doc examples/autofs
%doc examples/LDAP
%doc examples/libsmbclient
%doc examples/misc
%doc examples/printer-accounting
%doc examples/printing

##########

%files swat
%defattr(-,root,root)
%config(noreplace) %{_sysconfdir}/xinetd.d/swat
%dir %{_datadir}/swat
%{_datadir}/swat/*
%{_sbindir}/swat
%{_mandir}/man8/swat.8*
%attr(755,root,root) %{_libarchdir}/samba/*.msg

##########

%files client
%defattr(-,root,root)

%{_bindir}/rpcclient
%{_bindir}/smbcacls
%{_bindir}/findsmb
%{_bindir}/nmblookup
%{_bindir}/smbget
%{_bindir}/smbclient
%{_bindir}/smbprint
%{_bindir}/smbspool
%{_bindir}/smbtar
%{_bindir}/smbtree
%{_bindir}/sharesec
%{_bindir}/smbta-util

%{_mandir}/man8/smbspool.8*
%{_mandir}/man1/smbget.1*
%{_mandir}/man5/smbgetrc.5*
%{_mandir}/man1/findsmb.1*
%{_mandir}/man1/nmblookup.1*
%{_mandir}/man1/rpcclient.1*
%{_mandir}/man1/smbcacls.1*
%{_mandir}/man1/smbclient.1*
%{_mandir}/man1/smbtar.1*
%{_mandir}/man1/smbtree.1*
%{_mandir}/man1/sharesec.1*

##########

%files common
%defattr(-,root,root)
%dir %{_sysconfdir}/samba
%dir %{_libarchdir}/samba
%dir %{_libarchdir}/samba/charset
%config(noreplace) %{_sysconfdir}/samba/smb.conf
%config(noreplace) %{_sysconfdir}/samba/lmhosts
%attr(755,root,root) %config %{initdir}/winbind

%attr(755,root,root) /%{_libarch}/libnss_winbind.so
%attr(755,root,root) /%{_libarch}/libnss_winbind.so.2
%attr(755,root,root) /%{_libarch}/security/pam_winbind.so
%attr(755,root,root) /%{_libarch}/security/pam_smbpass.so
/usr/share/locale/*/LC_MESSAGES/pam_winbind.mo
/usr/share/locale/*/LC_MESSAGES/net.mo

%{_libarchdir}/samba/charset/CP437.so
%{_libarchdir}/samba/charset/CP850.so
%{_libarchdir}/samba/idmap/ad.so
%{_libarchdir}/samba/idmap/rid.so
%{_libarchdir}/samba/idmap/tdb2.so
%{_libarchdir}/samba/idmap/autorid.so
%{_libarchdir}/samba/lowcase.dat
%{_libarchdir}/samba/nss_info/rfc2307.so
%{_libarchdir}/samba/nss_info/sfu.so
%{_libarchdir}/samba/nss_info/sfu20.so
%{_libarchdir}/samba/upcase.dat
%{_libarchdir}/samba/valid.dat

%{_includedir}/libsmbclient.h
%{_libarchdir}/libsmbclient.*
%{_includedir}/smb_share_modes.h
%{_libarchdir}/libsmbsharemodes.so
%{_libarchdir}/libsmbsharemodes.so.0

%{_includedir}/netapi.h
%{_includedir}/wbclient.h
%{_includedir}/talloc.h
%{_includedir}/tdb.h
%{_libarchdir}/libnetapi.so
%{_libarchdir}/libnetapi.so.0
%{_libarchdir}/libtalloc.so
%{_libarchdir}/libtalloc.so.2
%{_libarchdir}/libtdb.so
%{_libarchdir}/libtdb.so.1
%{_libarchdir}/libwbclient.so
%{_libarchdir}/libwbclient.so.0

%{_libarchdir}/krb5/plugins/libkrb5/winbind_krb5_locator.so

%{_sbindir}/winbind

%{_sbindir}/winbindd
%{_bindir}/testparm
%{_bindir}/smbpasswd
%{_bindir}/profiles
%{_bindir}/net
%{_bindir}/wbinfo
%{_bindir}/ntlm_auth
%{_bindir}/pdbedit
%{_bindir}/smbcquotas
%{_bindir}/dbwrap_tool
%{_bindir}/dbwrap_torture

%{_mandir}/man1/ntlm_auth.1*
%{_mandir}/man1/profiles.1*
%{_mandir}/man1/smbcquotas.1*
%{_mandir}/man1/testparm.1*
%{_mandir}/man5/smb.conf.5*
%{_mandir}/man5/lmhosts.5*
%{_mandir}/man8/smbpasswd.8*
%{_mandir}/man1/wbinfo.1*
%{_mandir}/man8/winbindd.8*
%{_mandir}/man8/net.8*
%{_mandir}/man8/pam_winbind.8*
%{_mandir}/man7/libsmbclient.7*
%{_mandir}/man1/ldbadd.1*
%{_mandir}/man1/ldbdel.1*
%{_mandir}/man1/ldbedit.1*
%{_mandir}/man1/ldbmodify.1*
%{_mandir}/man1/ldbsearch.1*
%{_mandir}/man1/ldbrename.1*
%{_mandir}/man7/winbind_krb5_locator.7*
%{_mandir}/man8/idmap_*.8*

%ifarch x86_64 ppc64
%files winbind-32bit
%attr(755,root,root) /lib/libnss_winbind.so
%attr(755,root,root) /lib/libnss_winbind.so.2
%attr(755,root,root) /usr/lib/libtalloc.so
%attr(755,root,root) /usr/lib/libtalloc.so.2
%attr(755,root,root) /usr/lib/libtdb.so
%attr(755,root,root) /usr/lib/libtdb.so.1
%attr(755,root,root) /usr/lib/libwbclient.so
%attr(755,root,root) /usr/lib/libwbclient.so.0
%attr(755,root,root) /lib/security/pam_winbind.so
%endif



%changelog
* Fri Jan 16 2004 Gerald (Jerry) Carter <jerry@samba,org>
- Removed ChangeLog entries since they are kept in CVS



