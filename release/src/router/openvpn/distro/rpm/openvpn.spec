# OpenVPN spec file, used to drive rpmbuild

# OPTIONS
#
# Disable LZO
#   rpmbuild -tb [openvpn.x.tar.gz] --define 'without_lzo 1'
#
# Disable PAM plugin
#   rpmbuild -tb [openvpn.x.tar.gz] --define 'without_pam 1'
#
# Allow passwords to be read from files
#   rpmbuild -tb [openvpn.x.tar.gz] --define 'with_password_save 1'

Summary:	OpenVPN is a robust and highly flexible VPN daemon by James Yonan.
Name:		openvpn
Version:	2.3.4
Release:	1
URL:		http://openvpn.net/
Source0:	http://prdownloads.sourceforge.net/openvpn/%{name}-%{version}.tar.gz

License:	GPL
Group:		Applications/Internet
Vendor:		James Yonan <jim@yonan.net>
Packager:	James Yonan <jim@yonan.net>
BuildRoot:	%{_tmppath}/%{name}-%(id -un)

#
# Include dependencies manually
#

AutoReq: 0

BuildRequires:	openssl-devel >= 0.9.7
Requires:	openssl       >= 0.9.7

%if "%{_vendor}" == "Mandrakesoft"
%{!?without_lzo:BuildRequires:	liblzo1-devel >= 1.07}
%{!?without_lzo:Requires:	liblzo1       >= 1.07}
%else
%if "%{_vendor}" == "MandrakeSoft"
%{!?without_lzo:BuildRequires:	liblzo1-devel >= 1.07}
%{!?without_lzo:Requires:	liblzo1       >= 1.07}
%else
%{!?without_lzo:BuildRequires:	lzo-devel >= 1.07}
%{!?without_lzo:Requires:	lzo       >= 1.07}
%endif
%endif

%{!?without_pam:BuildRequires:	pam-devel}
%{!?without_pam:Requires:	pam}

%{?with_pkcs11:BuildRequires:	pkcs11-helper-devel}
%{?with_pkcs11:Requires:	pkcs11-helper}

#
# Description
#

%description
OpenVPN is a robust and highly flexible VPN daemon by James Yonan.
OpenVPN supports SSL/TLS security,
ethernet bridging,
TCP or UDP tunnel transport through proxies or NAT,
support for dynamic IP addresses and DHCP,
scalability to hundreds or thousands of users,
and portability to most major OS platforms.

%package devel
Summary:	OpenVPN is a robust and highly flexible VPN daemon by James Yonan.
Group:		Applications/Internet
Requires:	%{name}
%description devel
Development support for OpenVPN.

#
# Define vendor type
#

%if "%{_vendor}" == "suse" || "%{_vendor}" == "pc"
%define VENDOR SuSE
%else
%define VENDOR %_vendor
%endif

#
# Other definitions
#

%define debug_package %{nil}

#
# Build OpenVPN binary
#

%prep
%setup -q

%build
%configure \
	--disable-dependency-tracking \
	--docdir="%{_docdir}/%{name}-%{version}" \
	%{?with_password_save:--enable-password-save} \
	%{!?without_lzo:--enable-lzo} \
	%{?with_pkcs11:--enable-pkcs11} \
	%{?without_pam:--disable-plugin-auth-pam}
%__make

#
# Installation section
#

%install
[ %{buildroot} != "/" ] && rm -rf %{buildroot}
%__make install DESTDIR="%{buildroot}"

# Install init script
%if "%{VENDOR}" == "SuSE"
%__install -c -d -m 755 "%{buildroot}/etc/init.d"
%__install -c -m 755 "distro/rpm/%{name}.init.d.suse" "%{buildroot}/etc/init.d/%{name}"
%else
%__install -c -d -m 755 "%{buildroot}/etc/rc.d/init.d"
%__install -c -m 755 distro/rpm/%{name}.init.d.rhel "%{buildroot}/etc/rc.d/init.d/%{name}"
%endif

# Install /etc/openvpn
%__install -c -d -m 755 "%{buildroot}/etc/%{name}"

# Install extra %doc stuff
cp -r AUTHORS ChangeLog NEWS contrib/ sample/ \
	"%{buildroot}/%{_docdir}/%{name}-%{version}"

#
# Clean section
#

%clean
[ %{buildroot} != "/" ] && rm -rf "%{buildroot}"

#
# On Linux 2.4, make the device node
#

%post
case "`uname -r`" in
2.4*)
	/bin/mkdir /dev/net >/dev/null 2>&1
	/bin/mknod /dev/net/tun c 10 200 >/dev/null 2>&1
	;;
esac

#
# Handle the init script
#

/sbin/chkconfig --add %{name}
%if "%{VENDOR}" == "SuSE"
/etc/init.d/openvpn restart
%else
/sbin/service %{name} condrestart
%endif
%preun
if [ "$1" = 0 ]
then
	%if "%{VENDOR}" == "SuSE"
	/etc/init.d/openvpn stop
	%else
	/sbin/service %{name} stop
	%endif
	/sbin/chkconfig --del %{name}
fi

#
# Files section
#
# don't use %doc as old rpmbuild removes it[1].
# [1] http://rpm.org/ticket/836

%files
%defattr(-,root,root)
%{_mandir}
%{_sbindir}/%{name}
%{_libdir}/%{name}
%{_docdir}/%{name}-%{version}
%dir /etc/%{name}
%if "%{VENDOR}" == "SuSE"
/etc/init.d/%{name}
%else
/etc/rc.d/init.d/%{name}
%endif

%files devel
%defattr(-,root,root)
%{_includedir}/*

%changelog
* Thu Jul 30 2009 David Sommerseth <dazo@users.sourceforge.net>
- Removed management/ directory from %doc

* Thu Dec 14 2006 Alon Bar-Lev
- Added with_pkcs11

* Mon Aug 2 2005 James Yonan
- Fixed build problem with --define 'without_pam 1'

* Mon Apr 4 2005 James Yonan
- Moved some files from /usr/share/openvpn to %doc for compatibility
  with Dag Wieers' RPM repository

* Sat Mar 12 2005 Tom Walsh
- Added MandrakeSoft liblzo1 require

* Fri Dec 10 2004 James Yonan
- Added AutoReq: 0 for manual dependencies

* Fri Dec 10 2004 James Yonan
- Packaged the plugins

* Sun Nov 7 2004 Umberto Nicoletti
- SuSE support

* Wed Aug 18 2004 Bishop Clark (LC957) <bishop@platypus.bc.ca>
- restrict what we claim in /etc/ to avoid ownership conflicts

* Sun Feb 23 2003 Matthias Andree <matthias.andree@gmx.de> 1.3.2.14-1.
- Have the version number filled in by autoconf.

* Wed Jul 10 2002 James Yonan <jim@yonan.net> 1.3.1-1
- Fixed %preun to only remove service on final uninstall

* Mon Jun 17 2002 bishop clark (LC957) <bishop@platypus.bc.ca> 1.2.2-1
- Added condrestart to openvpn.spec & openvpn.init.

* Wed May 22 2002 James Yonan <jim@yonan.net> 1.2.0-1
- Added mknod for Linux 2.4.

* Wed May 15 2002 Doug Keller <dsk@voidstar.dyndns.org> 1.1.1.16-2
- Added init scripts
- Added conf file support

* Mon May 13 2002 bishop clark (LC957) <bishop@platypus.bc.ca> 1.1.1.14-1
- Added new directories for config examples and such

* Sun May 12 2002 bishop clark (LC957) <bishop@platypus.bc.ca> 1.1.1.13-1
- Updated buildroot directive and cleanup command
- added easy-rsa utilities

* Mon Mar 25 2002 bishop clark (LC957) <bishop@platypus.bc.ca> 1.0-1
- Initial build.
