
%define main_release 5
%define alpha_version 5
%define samba_version 4.0.0alpha%{alpha_version}
%define tarball_name samba-4.0.0alpha%{alpha_version}

#Set what versions we require for tdb and talloc
%define tdb_version 1.1.1
%define talloc_version 1.2.0

%{!?python_sitearch: %define python_sitearch %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib(1)")}

Summary: The Samba4 CIFS and AD client and server suite
Name: samba4
Version: 4.0.0
Release: 0.%{main_release}.alpha%{alpha_version}%{?dist}
License: GPLv3+ and LGPLv3+
Group: System Environment/Daemons
URL: http://www.samba.org/

Source: http://download.samba.org/samba/ftp/samba4/%{tarball_name}.tar.gz

# To be removed when samba4 alpha6 is released
# From http://git.samba.org/?p=samba.git;a=commitdiff;h=7ca421eb32bed3c400f863b654712d922c82bfb9
Patch0: cplusplus-headers.patch

# Red Hat specific replacement-files
Source1: %{name}.log
Source4: %{name}.sysconfig
Source5: %{name}.init

Requires(pre): %{name}-common = %{version}-%{release}
Requires: pam >= 0:0.64
Requires: logrotate >= 0:3.4
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
Requires(pre): /usr/sbin/groupadd
Requires(post): /sbin/chkconfig, /sbin/service
Requires(preun): /sbin/chkconfig, /sbin/service
BuildRequires: pam-devel, readline-devel, ncurses-devel, libacl-devel, e2fsprogs-devel
BuildRequires: popt-devel, libattr-devel, libaio-devel, sed
BuildRequires:  perl(ExtUtils::MakeMaker)
BuildRequires: libtalloc-devel >= %{talloc_version}
BuildRequires: libtdb-devel >= %{tdb_version}

%description

Samba 4 is the ambitious next version of the Samba suite that is being
developed in parallel to the stable 3.0 series. The main emphasis in
this branch is support for the Active Directory logon protocols used
by Windows 2000 and above.

%package client
Summary: Samba client programs
Group: Applications/System
Requires: %{name}-common = %{version}-%{release}
Requires: %{name}-libs = %{version}-%{release}

%description client
The %{name}-client package provides some SMB/CIFS clients to complement
the built-in SMB/CIFS filesystem in Linux. These clients allow access
of SMB/CIFS shares and printing to SMB/CIFS printers.

%package libs
Summary: Samba libraries
Group: Applications/System

%description libs
The %{name}-libs package  contains the libraries needed by programs 
that link against the SMB, RPC and other protocols provided by the Samba suite.

%package python
Summary: Samba python libraries
Group: Applications/System
Requires: %{name}-libs = %{version}-%{release}

%description python
The %{name}-python package contains the python libraries needed by programs 
that use SMB, RPC and other Samba provided protocols in python programs/

%package devel
Summary: Developer tools for Samba libraries
Group: Development/Libraries
Requires: %{name}-libs = %{version}-%{release}

%description devel
The %{name}-devel package contains the header files for the libraries
needed to develop programs that link against the SMB, RPC and other
libraries in the Samba suite.

%package pidl
Summary: Perl IDL compiler
Group: Development/Tools
Requires: perl(:MODULE_COMPAT_%(eval "`%{__perl} -V:version`"; echo $version))

%description pidl
The %{name}-pidl package contains the Perl IDL compiler used by Samba
and Wireshark to parse IDL and similar protocols

%package common
Summary: Files used by both Samba servers and clients
Group: Applications/System
Requires: %{name}-libs = %{version}-%{release}

%description common
%{Name}-common provides files necessary for both the server and client
packages of Samba.

%package winbind
Summary: Samba winbind
Group: Applications/System
Requires: %{name} = %{version}-%{release}

%description winbind
The samba-winbind package provides the winbind NSS library, and some
client tools.  Winbind enables Linux to be a full member in Windows
domains and to use Windows user and group accounts on Linux.


%prep
# TAG: change for non-pre
%setup -q -n %{tarball_name}
#%setup -q

# copy Red Hat specific scripts

# Upstream patches
%patch0 -p1 -b .

mv source/VERSION source/VERSION.orig
sed -e 's/SAMBA_VERSION_VENDOR_SUFFIX=$/&%{release}/' < source/VERSION.orig > source/VERSION
cd source
script/mkversion.sh
cd ..

%build
cd source

%configure \
	--with-fhs \
	--with-lockdir=/var/lib/%{name} \
	--with-piddir=/var/run \
	--with-privatedir=/var/lib/%{name}/private \
	--with-logfilebase=/var/log/%{name} \
	--sysconfdir=%{_sysconfdir}/%{name} \
	--with-winbindd-socket-dir=/var/run/winbind \
	--with-ntp-signd-socket-dir=/var/run/ntp_signd \
	--disable-gnutls

#Build PIDL for installation into vendor directories before 'make proto' gets to it
(cd pidl && %{__perl} Makefile.PL INSTALLDIRS=vendor )

#Builds using PIDL the IDL and many other things 
make proto

make everything

%install
rm -rf $RPM_BUILD_ROOT

cd source

#Don't call 'make install' as we want to call out to the PIDL install manually 
make install DESTDIR=%{buildroot}

#Undo the PIDL install, we want to try again with the right options
rm -rf $RPM_BUILD_ROOT/%{_libdir}/perl5
rm -rf $RPM_BUILD_ROOT/%{_datadir}/perl5

#Install PIDL
( cd pidl && make install PERL_INSTALL_ROOT=$RPM_BUILD_ROOT )

#Clean out crap left behind by the Pidl install
find $RPM_BUILD_ROOT -type f -name .packlist -exec rm -f {} \;
find $RPM_BUILD_ROOT -depth -type d -exec rmdir {} 2>/dev/null \;

cd ..

mkdir -p $RPM_BUILD_ROOT/%{_initrddir}
mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/logrotate.d
mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/%{name}
mkdir -p $RPM_BUILD_ROOT/var/run/winbindd
mkdir -p $RPM_BUILD_ROOT/var/run/ntp_signd
mkdir -p $RPM_BUILD_ROOT/var/lib/%{name}/winbindd_privileged
mkdir -p $RPM_BUILD_ROOT/var/log/%{name}/
mkdir -p $RPM_BUILD_ROOT/var/log/%{name}/old

mkdir -p $RPM_BUILD_ROOT/var/lib/%{name}
mkdir -p $RPM_BUILD_ROOT/var/lib/%{name}/private
mkdir -p $RPM_BUILD_ROOT/var/lib/%{name}/sysvol

mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig

# Install other stuff
install -m755 %{SOURCE5} $RPM_BUILD_ROOT/%{_initrddir}/%{name}
install -m644 %{SOURCE1} $RPM_BUILD_ROOT/%{_sysconfdir}/logrotate.d/%{name}

# winbind
mkdir -p $RPM_BUILD_ROOT/%{_lib}
ln -sf ../%{_libdir}/libnss_winbind.so  $RPM_BUILD_ROOT/%{_lib}/libnss_winbind.so.2 

# libs {
mkdir -p $RPM_BUILD_ROOT%{_libdir} $RPM_BUILD_ROOT%{_includedir}

# }

install -m644 %{SOURCE4} $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig/%{name}

#clean out some stuff we don't want in the Fedora package
rm $RPM_BUILD_ROOT/%{_bindir}/autoidl.py*
rm $RPM_BUILD_ROOT/%{_bindir}/epdump.py*
rm $RPM_BUILD_ROOT/%{_bindir}/gentest
rm $RPM_BUILD_ROOT/%{_bindir}/locktest
rm $RPM_BUILD_ROOT/%{_bindir}/masktest
rm $RPM_BUILD_ROOT/%{_bindir}/minschema.py*
rm $RPM_BUILD_ROOT/%{_bindir}/rpcclient
rm $RPM_BUILD_ROOT/%{_bindir}/samba3dump
rm $RPM_BUILD_ROOT/%{_bindir}/setnttoken
rm $RPM_BUILD_ROOT/%{_bindir}/getntacl
rm $RPM_BUILD_ROOT/%{_datadir}/samba/js/base.js

#This makes the right links, as rpmlint requires that the
#ldconfig-created links be recorded in the RPM.
/sbin/ldconfig -N -n $RPM_BUILD_ROOT/%{_libdir}

#Fix up permission on perl install
%{_fixperms} $RPM_BUILD_ROOT/%{perl_vendorlib}

#Fix up permissions in source tree, for debuginfo
find source/heimdal -type f | xargs chmod -x

%clean
rm -rf $RPM_BUILD_ROOT

%pre
getent group wbpriv >/dev/null || groupadd -g 88 wbpriv
exit 0

%post
/sbin/chkconfig --add %{name}
if [ "$1" -ge "1" ]; then
	/sbin/service %{name} condrestart >/dev/null 2>&1 || :
fi
exit 0

%preun
if [ $1 = 0 ] ; then
	/sbin/service %{name} stop >/dev/null 2>&1 || :
	/sbin/chkconfig --del %{name}
fi
exit 0

%post libs -p /sbin/ldconfig

%postun libs -p /sbin/ldconfig


%files
%defattr(-,root,root)
%{_sbindir}/smbd
%{_bindir}/smbstatus

%attr(755,root,root) %{_initrddir}/%{name}
%config(noreplace) %{_sysconfdir}/logrotate.d/%{name}
%dir %{_datadir}/samba/setup
%{_datadir}/samba/setup/*
%dir /var/lib/%{name}/sysvol
%config(noreplace) %{_sysconfdir}/sysconfig/%{name}
%attr(0700,root,root) %dir /var/log/%{name}
%attr(0700,root,root) %dir /var/log/%{name}/old

%files libs
%defattr(-,root,root)
%doc WHATSNEW.txt NEWS PFIF.txt
%dir %{_datadir}/samba
%{_datadir}/samba/*.dat
%{_libdir}/*.so.*
#Only needed if Samba's build produces plugins
#%{_libdir}/samba
%dir %{_sysconfdir}/%{name}
#Need to mark this as being owned by Samba, but it is normally created
#by the provision script, which runs best if there is no existing
#smb.conf
#%config(noreplace) %{_sysconfdir}/%{name}/smb.conf

%files winbind
%defattr(-,root,root)
%{_bindir}/ntlm_auth
%{_bindir}/wbinfo
%{_libdir}/libnss_winbind.so
/%{_lib}/libnss_winbind.so.2
%dir /var/run/winbindd
%attr(750,root,wbpriv) %dir /var/lib/%{name}/winbindd_privileged

%files python
%defattr(-,root,root)
%{python_sitearch}/*

%files devel
%defattr(-,root,root)
%{_libdir}/libdcerpc.so
%{_libdir}/libdcerpc_atsvc.so
%{_libdir}/libdcerpc_samr.so
%{_libdir}/libgensec.so
%{_libdir}/libldb.so
%{_libdir}/libndr.so
%{_libdir}/libregistry.so
%{_libdir}/libsamba-hostconfig.so
%{_libdir}/libtorture.so

%{_libdir}/pkgconfig
%{_includedir}/*
%{_bindir}/ndrdump
%{_bindir}/nsstest

%files pidl
%defattr(-,root,root,-)
%{perl_vendorlib}/*
%{_mandir}/man1/pidl*
%{_mandir}/man3/Parse*
%attr(755,root,root) %{_bindir}/pidl

%files client
%defattr(-,root,root)
%{_bindir}/nmblookup
%{_bindir}/smbclient
%{_bindir}/cifsdd

%files common
%defattr(-,root,root)
%{_bindir}/net
%{_bindir}/testparm
%{_bindir}/ldbadd
%{_bindir}/ldbdel
%{_bindir}/ldbedit
%{_bindir}/ldbmodify
%{_bindir}/ldbsearch
%{_bindir}/ldbrename
%{_bindir}/ad2oLschema
%{_bindir}/oLschema2ldif
%{_bindir}/regdiff
%{_bindir}/regpatch
%{_bindir}/regshell
%{_bindir}/regtree
%{_bindir}/subunitrun
%{_bindir}/smbtorture

%dir /var/lib/%{name}
%attr(700,root,root) %dir /var/lib/%{name}/private
# We don't want to put a smb.conf in by default, provision should create it
#%config(noreplace) %{_sysconfdir}/%{name}/smb.conf

%doc COPYING
%doc WHATSNEW.txt

%changelog
* Fri Aug 29 2008 Andrew Bartlett <abartlet@samba.org> - 0:4.0.0-0.5.alpha5.fc10
- Fix licence tag (the binaries are built into a GPLv3 whole, so the BSD licence need not be mentioned)

* Fri Jul 25 2008 Andrew Bartlett <abartlet@samba.org> - 0:4.0.0-0.4.alpha5.fc10
- Remove talloc and tdb dependency (per https://bugzilla.redhat.com/show_bug.cgi?id=453083)
- Fix deps on chkconfig and service to main pkg (not -common) 
  (per https://bugzilla.redhat.com/show_bug.cgi?id=453083)

* Mon Jul 21 2008 Brad Hards <bradh@frogmouth.ent> - 0:4.0.0-0.3.alpha5.fc10
- Use --sysconfdir instead of --with-configdir
- Add patch for C++ header compatibility

* Mon Jun 30 2008 Andrew Bartlett <abartlet@samba.org> - 0:4.0.0-0.2.alpha5.fc9
- Update per review feedback
- Update for alpha5

* Thu Jun 26 2008 Andrew Bartlett <abartlet@samba.org> - 0:4.0.0-0.1.alpha4.fc9
- Rework Fedora's Samba 3.2.0-1.rc2.16 spec file for Samba4
