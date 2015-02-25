%define initdir %{_sysconfdir}/rc.d/init.d
%define auth %(test -f /etc/pam.d/system-auth && echo /etc/pam.d/system-auth || echo)
%define this_is_redhat  %(test -e /etc/redhat-release && echo 1 || echo 0)
%if %{this_is_redhat} > 0
%define rhel_ver %(grep "release" /etc/redhat-release | sed %"s/^[^0-9]*\\([0-9]*\\).*/\\1/g")
%else
%define rhel_ver 0
%endif

Summary: Samba SMB client and server
Vendor: Samba Team
Packager: Samba Team <samba@samba.org>
Name:         samba
Version:      3.6.25
Release:      1
Epoch:        0
License: GNU GPL version 3
Group: System Environment/Daemons
URL: http://www.samba.org/

Source: samba-%{version}.tar.bz2

# Don't depend on Net::LDAP
Source998: filter-requires-samba.sh
Source999: setup.tar.bz2

Prereq: /sbin/chkconfig /bin/mktemp /usr/bin/killall
Prereq: fileutils sed /etc/init.d

Requires: pam >= 0.64 %{auth} 
Requires: samba-common = %{version}-%{release}
Requires: logrotate >= 3.4 initscripts >= 5.54-1
Provides: samba = %{version}

Prefix: /usr
BuildRoot: %{_tmppath}/%{name}-%{version}-root
BuildRequires: pam-devel, readline-devel, fileutils, libacl-devel, openldap-devel, krb5-devel, cups-devel

%if %{rhel_ver} > 4
BuildRequires:  keyutils-libs-devel
%else
BuildRequires:  keyutils-devel
%endif

# Working around perl dependency problem from docs
%define __perl_requires %{SOURCE998}

# rpm screws up the arch lib dir when using --target on RHEL5
%ifarch i386 i486 i586 i686 ppc s390
%define _libarch lib
%else
%define _libarch %_lib
%endif

%define _libarchdir /usr/%{_libarch}


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


#######################################################################
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


#######################################################################
%package doc
Summary:      Samba Documentation
Group:        Documentation/Other
Provides:     samba-doc = %{version}-%{release}
Prereq:       /usr/bin/find /bin/rm /usr/bin/xargs

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
if [ "$(which ccache 2> /dev/null)" != "" ]; then
	CC="ccache gcc"
else
	CC="gcc"
fi 

## always run autogen.sh
./autogen.sh

## ignore insufficiently linked libreadline (RH bugzilla #499837):
export LDFLAGS="$LDFLAGS -Wl,--allow-shlib-undefined,--no-as-needed"

CC="$CC" CFLAGS="$RPM_OPT_FLAGS $EXTRA -D_GNU_SOURCE" ./configure \
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
	--enable-cups \
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
	--with-shared-modules=idmap_rid,idmap_ad,idmap_hash,idmap_adex \
	--with-syslog \
	--with-utmp \
	--with-dnsupdate

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


make all modules pam_smbpass

# Remove some permission bits to avoid to many dependencies
cd ..
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
mkdir -p $RPM_BUILD_ROOT%{_mandir}
mkdir -p $RPM_BUILD_ROOT%{_prefix}/{bin,sbin}
mkdir -p $RPM_BUILD_ROOT%{_prefix}/lib
mkdir -p $RPM_BUILD_ROOT/sbin
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/{logrotate.d,pam.d,samba}
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/{pam.d,logrotate.d}
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/rc.d/init.d
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/{samba,sysconfig}
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/xinetd.d
mkdir -p $RPM_BUILD_ROOT/var/lib/samba/winbindd_privileged
mkdir -p $RPM_BUILD_ROOT/var/{log,run/winbindd,spool}/samba

cd source3
make DESTDIR=$RPM_BUILD_ROOT \
        install
cd ..

# NSS winbind support
install -m 755 nsswitch/libnss_winbind.so $RPM_BUILD_ROOT/%{_libarch}/libnss_winbind.so.2
install -m 755 nsswitch/libnss_wins.so $RPM_BUILD_ROOT/%{_libarch}/libnss_wins.so.2
( cd $RPM_BUILD_ROOT/%{_libarch};
  ln -sf libnss_winbind.so.2  libnss_winbind.so;
  ln -sf libnss_wins.so.2  libnss_wins.so )

## cleanup
/bin/rm -rf $RPM_BUILD_ROOT/usr/lib*/samba/security

# Install the miscellany
echo 127.0.0.1 localhost > $RPM_BUILD_ROOT%{_sysconfdir}/samba/lmhosts

install -m644 setup/samba.log $RPM_BUILD_ROOT%{_sysconfdir}/logrotate.d/samba
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

## don't duplicate the docs.  These are installed by/with SWAT
rm -rf docs/htmldocs
rm -rf docs/manpages
( cd docs; ln -s %{_prefix}/share/swat/help htmldocs )

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

%preun
if [ $1 = 0 ] ; then
    /sbin/chkconfig --del smb
    /sbin/chkconfig --del winbind
    # rm -rf /var/log/samba/* /var/lib/samba/*
    /sbin/service smb stop >/dev/null 2>&1
fi
exit 0

%postun
if [ "$1" -ge "1" ]; then
	%{initdir}/smb restart >/dev/null 2>&1
fi	


%post swat
# Add swat entry to /etc/services if not already there.
if [ ! "`grep ^\s**swat /etc/services`" ]; then
        echo 'swat        901/tcp     # Add swat service used via inetd' >> /etc/services
fi

%post common
/sbin/ldconfig

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
%attr(755,root,root) %config %{initdir}/winbind
%config(noreplace) %{_sysconfdir}/logrotate.d/samba
%config(noreplace) %{_sysconfdir}/pam.d/samba

%attr(0755,root,root) %dir /var/log/samba
%attr(0755,root,root) %dir /var/lib/samba
%attr(1777,root,root) %dir /var/spool/samba

%{_sbindir}/samba
%{_sbindir}/winbind

%{_sbindir}/smbd
%{_sbindir}/nmbd
%{_sbindir}/winbindd

%{_bindir}/mksmbpasswd.sh
%{_bindir}/smbcontrol
%{_bindir}/smbstatus
%{_bindir}/smbta-util
%{_bindir}/tdbbackup
%{_bindir}/tdbtool
%{_bindir}/tdbdump
%{_bindir}/tdbrestore
%{_bindir}/wbinfo
%{_bindir}/ntlm_auth
%{_bindir}/pdbedit
%{_bindir}/eventlogadm

%{_libarchdir}/samba/idmap/*.so
%{_libarchdir}/samba/nss_info/*.so
%{_libarchdir}/samba/vfs/*.so
%{_libarchdir}/samba/auth/*.so

%{_mandir}/man1/smbcontrol.1*
%{_mandir}/man1/smbstatus.1*
%{_mandir}/man1/vfstest.1*
%{_mandir}/man5/smbpasswd.5*
%{_mandir}/man7/samba.7*
%{_mandir}/man7/winbind_krb5_locator.7*
%{_mandir}/man8/nmbd.8*
%{_mandir}/man8/pdbedit.8*
%{_mandir}/man8/smbd.8*
%{_mandir}/man8/tdbbackup.8*
%{_mandir}/man8/tdbdump.8*
%{_mandir}/man8/tdbtool.8*
%{_mandir}/man8/eventlogadm.8*
%{_mandir}/man8/winbindd.8*
%{_mandir}/man1/ntlm_auth.1*
%{_mandir}/man1/wbinfo.1*
%{_mandir}/man1/dbwrap_*.1*
%{_mandir}/man8/vfs_*.8*
%{_mandir}/man8/idmap_*.8*


##########

%files doc
%defattr(-,root,root)
%doc README COPYING Manifest 
%doc WHATSNEW.txt Roadmap
%doc docs
%doc examples/autofs examples/LDAP examples/libsmbclient examples/misc examples/printer-accounting
%doc examples/printing
%doc %{_datadir}/swat/help
%doc %{_datadir}/swat/using_samba

##########

%files swat
%defattr(-,root,root)
%config(noreplace) %{_sysconfdir}/xinetd.d/swat
%dir %{_datadir}/swat
%{_datadir}/swat/include
%{_datadir}/swat/images
%{_datadir}/swat/lang
%{_sbindir}/swat
%{_mandir}/man8/swat.8*

##########

%files client
%defattr(-,root,root)

%{_bindir}/rpcclient
%{_bindir}/smbcacls
%{_bindir}/sharesec
%{_bindir}/findsmb
%{_bindir}/smbcquotas
%{_bindir}/nmblookup
%{_bindir}/smbget
%{_bindir}/smbclient
%{_bindir}/smbprint
%{_bindir}/smbspool
%{_bindir}/smbtar
%{_bindir}/net
%{_bindir}/smbtree

%{_mandir}/man8/smbspool.8*
%{_mandir}/man1/smbget.1*
%{_mandir}/man5/smbgetrc.5*
%{_mandir}/man1/findsmb.1*
%{_mandir}/man1/nmblookup.1*
%{_mandir}/man1/rpcclient.1*
%{_mandir}/man1/smbcacls.1*
%{_mandir}/man1/sharesec.1*
%{_mandir}/man1/smbclient.1*
%{_mandir}/man1/smbtar.1*
%{_mandir}/man1/smbtree.1*
%{_mandir}/man8/net.8*
%{_mandir}/man1/smbcquotas.1*

##########

%files common
%defattr(-,root,root)
%dir %{_sysconfdir}/samba
%dir %{_libarchdir}/samba
%dir %{_libarchdir}/samba/charset
%config(noreplace) %{_sysconfdir}/samba/smb.conf
%config(noreplace) %{_sysconfdir}/samba/lmhosts

%attr(755,root,root) /%{_libarch}/libnss_wins.so*
%attr(755,root,root) /%{_libarch}/libnss_winbind.so*
%attr(755,root,root) /%{_libarch}/security/pam_winbind.so
%attr(755,root,root) /%{_libarch}/security/pam_smbpass.so
/usr/share/locale/de/LC_MESSAGES/net.mo
/usr/share/locale/de/LC_MESSAGES/pam_winbind.mo
/usr/share/locale/ar/LC_MESSAGES/pam_winbind.mo
/usr/share/locale/cs/LC_MESSAGES/pam_winbind.mo
/usr/share/locale/da/LC_MESSAGES/pam_winbind.mo
/usr/share/locale/es/LC_MESSAGES/pam_winbind.mo
/usr/share/locale/fi/LC_MESSAGES/pam_winbind.mo
/usr/share/locale/fr/LC_MESSAGES/pam_winbind.mo
/usr/share/locale/hu/LC_MESSAGES/pam_winbind.mo
/usr/share/locale/it/LC_MESSAGES/pam_winbind.mo
/usr/share/locale/ja/LC_MESSAGES/pam_winbind.mo
/usr/share/locale/ko/LC_MESSAGES/pam_winbind.mo
/usr/share/locale/nb/LC_MESSAGES/pam_winbind.mo
/usr/share/locale/nl/LC_MESSAGES/pam_winbind.mo
/usr/share/locale/pl/LC_MESSAGES/pam_winbind.mo
/usr/share/locale/pt_BR/LC_MESSAGES/pam_winbind.mo
/usr/share/locale/ru/LC_MESSAGES/pam_winbind.mo
/usr/share/locale/sv/LC_MESSAGES/pam_winbind.mo
/usr/share/locale/zh_CN/LC_MESSAGES/pam_winbind.mo
/usr/share/locale/zh_TW/LC_MESSAGES/pam_winbind.mo

%{_includedir}/libsmbclient.h
%{_libarchdir}/libsmbclient.*
%{_includedir}/smb_share_modes.h
%{_libarchdir}/libsmbsharemodes.*

%{_libarchdir}/samba/*.dat
%{_libarchdir}/samba/*.msg
%{_libarchdir}/samba/charset/*.so

%{_includedir}/netapi.h
%{_includedir}/wbclient.h
%{_includedir}/talloc.h
%{_includedir}/tdb.h
%{_libarchdir}/libnetapi.so*
%{_libarchdir}/libtalloc.so*
%{_libarchdir}/libtdb.so*
%{_libarchdir}/libwbclient.so*

%{_bindir}/testparm
%{_bindir}/smbpasswd
%{_bindir}/profiles

%{_mandir}/man1/profiles.1*
%{_mandir}/man1/testparm.1*
%{_mandir}/man5/smb.conf.5*
%{_mandir}/man5/lmhosts.5*
%{_mandir}/man8/smbpasswd.8*
%{_mandir}/man5/pam_winbind.conf.5.*
%{_mandir}/man7/libsmbclient.7*
%{_mandir}/man8/smbta-util.8*
%{_mandir}/man8/pam_winbind.8*

%changelog
* Fri Jan 16 2004 Gerald (Jerry) Carter <jerry@samba,org>
- Removed ChangeLog entries since they are kept in CVS



