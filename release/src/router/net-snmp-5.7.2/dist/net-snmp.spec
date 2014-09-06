#
# 5.4+ enables Perl by default
#
%define netsnmp_embedded_perl 1
%define netsnmp_perl_modules 1
%define netsnmp_cflags ""

# ugly RHEL detector
# SuSE build service defines rhel_version, RHEL itself defines nothing
%if 0%{?rhel_version}
%define rhel %{?rhel_version}
%else
%define is_rhel %(grep -E "Red Hat Enterprise Linux|CentOS" /etc/redhat-release &>/dev/null && echo 1 || echo 0)
%if %{is_rhel}
%define rhel %(sed </etc/redhat-release -e 's/.*release \\(.\\).*/\\1/'  )
%endif
%endif

# because perl(Tk) is optional, automatic dependencies will never succeed:
%define _use_internal_dependency_generator 0
%define __find_requires %{_builddir}/net-snmp-%{version}/dist/find-requires
%define __find_provides /usr/lib/rpm/find-provides

#
# Check for -without embedded_perl
#
%{?_without_embedded_perl:%define netsnmp_embedded_perl 0}
#
# check for -without perl_modules
#
%{?_without_perl_modules:%define netsnmp_perl_modules 0}
#
# if embedded_perl or perl_modules specified, include some Perl stuff
#
%if 0%{?netsnmp_embedded_perl} || 0%{?netsnmp_perl_modules}
%define netsnmp_include_perl 1
%endif
Summary: Tools and servers for the SNMP protocol
Name: net-snmp
Version: 5.7.2
# update release for vendor release. (eg 1.fc6, 1.rh72, 1.ydl3, 1.ydl23)
Release: 1
URL: http://www.net-snmp.org/
License: BSDish
Group: System Environment/Daemons
Vendor: Net-SNMP project
Source: http://prdownloads.sourceforge.net/net-snmp/net-snmp-%{version}.tar.gz
Prereq: openssl
Obsoletes: cmu-snmp ucd-snmp ucd-snmp-utils
BuildRoot: /tmp/%{name}-root
Packager: The Net-SNMP Coders <http://sourceforge.net/projects/net-snmp/>
Requires: openssl, popt, rpm, zlib, bzip2-libs, elfutils-libelf, glibc
BuildRequires: perl, elfutils-libelf-devel, openssl-devel, bzip2-devel, rpm-devel
%if %{netsnmp_embedded_perl}
BuildRequires: perl(ExtUtils::Embed)
Requires: perl
%endif

%if 0%{?fedora}%{?rhel}
# Fedora & RHEL specific requires/provides
Provides: net-snmp-libs, net-snmp-utils
Obsoletes: net-snmp-libs, net-snmp-utils
Epoch: 2

%if 0%{?fedora} >= 9
Provides: net-snmp-gui
Obsoletes: net-snmp-gui
# newer fedoras need following macro to compile with new rpm
%define netsnmp_cflags "-D_RPM_4_4_COMPAT"
%else
BuildRequires: beecrypt-devel
%endif
%endif # RHEL or Fedora

%description

Net-SNMP provides tools and libraries relating to the Simple Network
Management Protocol including: An extensible agent, An SNMP library,
tools to request or set information from SNMP agents, tools to
generate and handle SNMP traps, etc.  Using SNMP you can check the
status of a network of computers, routers, switches, servers, ... to
evaluate the state of your network.

%if %{netsnmp_embedded_perl}
This package includes embedded Perl support within the agent.
%endif

%package devel
Group: Development/Libraries
Summary: The includes and static libraries from the Net-SNMP package.
AutoReqProv: no
Requires: net-snmp = %{epoch}:%{version}
Obsoletes: cmu-snmp-devel ucd-snmp-devel

%description devel
The net-snmp-devel package contains headers and libraries which are
useful for building SNMP applications, agents, and sub-agents.

%if %{netsnmp_include_perl}
%package perlmods
Group: System Environment/Libraries
Summary: The Perl modules provided with Net-SNMP
AutoReqProv: no
Requires: net-snmp = %{epoch}:%{version}, perl

%if 0%{?fedora}%{?rhel}
Provides: net-snmp-perl
Provides: perl(SNMP) perl(NetSNMP::OID)
Provides: perl(NetSNMP::ASN)
Provides: perl(NetSNMP::AnyData::Format::SNMP) perl(NetSNMP::AnyData::Storage::SNMP)
Provides: perl(NetSNMP::agent)
Provides: perl(NetSNMP::manager) perl(NetSNMP::TrapReceiver)
Provides: perl(NetSNMP::default_store) perl(NetSNMP::agent::default_store)
Obsoletes: net-snmp-perl
%endif

%description perlmods
Net-SNMP provides a number of Perl modules useful when using the SNMP
protocol.  Both client and agent support modules are provided.
%endif

%prep
%if %{netsnmp_embedded_perl} == 1 && %{netsnmp_perl_modules} == 0
echo "'-with embedded_perl' requires '-with perl_modules'"
exit 1
%endif
%setup -q

%build
%configure --with-defaults --with-sys-contact="Unknown" \
	--with-mib-modules="smux" \
	--with-sysconfdir="/etc/net-snmp"               \
	--enable-shared \
	%{?netsnmp_perl_modules: --with-perl-modules="INSTALLDIRS=vendor"} \
	%{!?netsnmp_perl_modules: --without-perl-modules} \
	%{?netsnmp_embedded_perl: --enable-embedded-perl} \
	%{!?netsnmp_embedded_perl: --disable-embedded-perl} \
	--with-cflags="$RPM_OPT_FLAGS %{netsnmp_cflags}"

make

%install
# ----------------------------------------------------------------------
# 'install' sets the current directory to _topdir/BUILD/{name}-{version}
# ----------------------------------------------------------------------
rm -rf $RPM_BUILD_ROOT

make DESTDIR=%{buildroot} install

# Remove 'snmpinform' from the temporary directory because it is a
# symbolic link, which cannot be handled by the rpm installation process.
%__rm -f $RPM_BUILD_ROOT%{_prefix}/bin/snmpinform
# install the init script
mkdir -p $RPM_BUILD_ROOT/etc/rc.d/init.d
perl -i -p -e 's@/usr/local/share/snmp/@/etc/snmp/@g;s@usr/local@%{_prefix}@g' dist/snmpd-init.d
install -m 755 dist/snmpd-init.d $RPM_BUILD_ROOT/etc/rc.d/init.d/snmpd

%if %{netsnmp_include_perl}
# unneeded Perl stuff
find $RPM_BUILD_ROOT/%{_libdir}/perl5/ -name Bundle -type d | xargs rm -rf
find $RPM_BUILD_ROOT/%{_libdir}/perl5/ -name perllocal.pod | xargs rm -f

# store a copy of installed Perl stuff.  It's too complex to predict
(xxdir=`pwd` && cd $RPM_BUILD_ROOT && find usr/lib*/perl5 -type f | sed 's/^/\//' > $xxdir/net-snmp-perl-files)
%endif

%post
# ----------------------------------------------------------------------
# The 'post' script is executed just after the package is installed.
# ----------------------------------------------------------------------
# Create the symbolic link 'snmpinform' after all other files have
# been installed.
%__rm -f $RPM_INSTALL_PREFIX/bin/snmpinform
%__ln_s $RPM_INSTALL_PREFIX/bin/snmptrap $RPM_INSTALL_PREFIX/bin/snmpinform

# run ldconfig
PATH="$PATH:/sbin" ldconfig -n $RPM_INSTALL_PREFIX/lib

%preun
# ----------------------------------------------------------------------
# The 'preun' script is executed just before the package is erased.
# ----------------------------------------------------------------------
# Remove the symbolic link 'snmpinform' before anything else, in case
# it is in a directory that rpm wants to remove (at present, it isn't).
%__rm -f $RPM_INSTALL_PREFIX/bin/snmpinform

%postun
# ----------------------------------------------------------------------
# The 'postun' script is executed just after the package is erased.
# ----------------------------------------------------------------------
PATH="$PATH:/sbin" ldconfig -n $RPM_INSTALL_PREFIX/lib

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)

# Install the following documentation in _defaultdocdir/{name}-{version}/
%doc AGENT.txt ChangeLog CodingStyle COPYING
%doc EXAMPLE.conf.def FAQ INSTALL NEWS PORTING TODO
%doc README README.agentx README.hpux11 README.krb5
%doc README.snmpv3 README.solaris README.thread README.win32
%doc README.aix README.osX README.tru64 README.irix README.agent-mibs
%doc README.Panasonic_AM3X.txt
	 
#%config(noreplace) /etc/net-snmp/snmpd.conf
	 
#%{_datadir}/snmp/snmpconf-data
%{_datadir}/snmp

%{_bindir}
%{_sbindir}
%{_mandir}/man1/*
# don't include Perl man pages, which start with caps
%{_mandir}/man3/[^A-Z]*
%{_mandir}/man5/*
%{_mandir}/man8/*
%{_libdir}/*.so*
/etc/rc.d/init.d/snmpd

%files devel
%defattr(-,root,root)

%{_includedir}
%{_libdir}/*.a
%{_libdir}/*.la

%if %{netsnmp_include_perl}
%files -f net-snmp-perl-files perlmods
%defattr(-,root,root)
%{_mandir}/man3/*::*
%{_mandir}/man3/SNMP*
%endif

%verifyscript
echo "No additional verification is done for net-snmp"

%changelog
* Thu Jul 26 2012 Dave Shield <D.T.Shield@liverpool.ac.uk>
- Additional "Provides:" to complete the list of perl modules
  Triggered by Bug ID #3540621

* Thu Oct  7 2010 Peter Green <peter.green@az-tek.co.uk>
- Modified RHEL detection to include CentOS.
- Added extra "Provides:" to the perlmods package definition;
  otherwise subsequent package installations that require certain
  Perl modules try to re-install RHEL/CentOS stock net-snmp

* Tue May  6 2008 Jan Safranek <jsafranek@users.sf.net>
- remove %{libcurrent}
- add openssl-devel to build requirements
- don't use Provides: unless necessary, let rpmbuild compute the provided 
  libraries

* Tue Jun 19 2007 Thomas Anders <tanders@users.sf.net>
- add "BuildRequires: perl-ExtUtils-Embed", e.g. for Fedora 7

* Wed Nov 23 2006 Thomas Anders <tanders@users.sf.net>
- fixes for 5.4 and 64-bit platforms
- enable Perl by default, but allow for --without perl_modules|embedded_perl
- add netsnmp_ prefix for local defines

* Fri Sep  1 2006 Thomas Anders <tanders@users.sf.net>
- Update to 5.4.dev
- introduce %{libcurrent}
- use new disman/event name
- add: README.aix README.osX README.tru64 README.irix README.agent-mibs
  README.Panasonic_AM3X.txt
- add new NetSNMP::agent::Support

* Fri Jan 13 2006 hardaker <hardaker@users.sf.net>
- Update to 5.3.0.1

* Wed Dec 28 2005 hardaker <hardaker@users.sf.net>
- Update to 5.3

* Tue Oct 28 2003 rs <rstory@users.sourceforge.net>
- fix conditional perl build after reading rpm docs

* Sat Oct  4 2003 rs <rstory@users.sourceforge.net> - 5.0.9-4
- fix to build without requiring arguments
- separate embedded perl and perl modules options
- fix fix for init.d script for non-/usr/local installation

* Fri Sep 26 2003 Wes Hardaker <hardaker@users.sourceforge.net>
- fix perl's UseNumeric
- fix init.d script for non-/usr/local installation

* Fri Sep 12 2003 Wes Hardaker <hardaker@users.sourceforge.net>
- fixes for 5.0.9's perl support

* Mon Sep 01 2003 Wes Hardaker <hardaker@users.sourceforge.net>
- added perl support

* Wed Oct 09 2002 Wes Hardaker <hardaker@users.sourceforge.net>
- Incorperated most of Mark Harig's better version of the rpm spec and Makefile

* Wed Oct 09 2002 Wes Hardaker <hardaker@users.sourceforge.net>
- Made it possibly almost usable.

* Mon Apr 22 2002 Robert Story <rstory@users.sourceforge.net>
- created
