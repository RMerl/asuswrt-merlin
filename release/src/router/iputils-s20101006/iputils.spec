#
# This spec file is for _testing_. You may use it (I do), but no warranty.
#

%define ssdate 020124
Summary: The ping program for checking to see if network hosts are alive.
Name: iputils
Version: 20%{ssdate}
Release: 1local
License: BSD
Group: System Environment/Daemons
Source0: iputils-ss%{ssdate}.tar.bz2
Prefix: %{_prefix}
BuildRoot: %{_tmppath}/%{name}-root
BuildPrereq: docbook-dtd31-sgml, perl
Requires: kernel >= 2.4.7

%description
The iputils package contains ping, a basic networking tool.  The ping
command sends a series of ICMP protocol ECHO_REQUEST packets to a
specified network host and can tell you if that machine is alive and
receiving network traffic.

%prep
%setup -q -n %{name}

%build
make CC=gcc3
make man
make html

%install
rm -rf ${RPM_BUILD_ROOT}

mkdir -p ${RPM_BUILD_ROOT}%{_sbindir}
mkdir -p ${RPM_BUILD_ROOT}/{bin,sbin}
install -c clockdiff		${RPM_BUILD_ROOT}%{_sbindir}/
%ifos linux
install -c arping               ${RPM_BUILD_ROOT}/sbin/
ln -s ../../sbin/arping ${RPM_BUILD_ROOT}%{_sbindir}/arping
install -c ping			${RPM_BUILD_ROOT}/bin/
%else
install -c arping		${RPM_BUILD_ROOT}%{_sbindir}/
install -c ping			${RPM_BUILD_ROOT}%{_sbindir}/
%endif
install -c ping6		${RPM_BUILD_ROOT}%{_sbindir}/
install -c rdisc		${RPM_BUILD_ROOT}%{_sbindir}/
install -c tracepath		${RPM_BUILD_ROOT}%{_sbindir}/
install -c tracepath6		${RPM_BUILD_ROOT}%{_sbindir}/
install -c traceroute6		${RPM_BUILD_ROOT}%{_sbindir}/

mkdir -p ${RPM_BUILD_ROOT}%{_mandir}/man8
install -c doc/*.8		${RPM_BUILD_ROOT}%{_mandir}/man8/

%clean
rm -rf ${RPM_BUILD_ROOT}

%files
%defattr(-,root,root)
%doc RELNOTES doc/*.html
%{_sbindir}/clockdiff
%ifos linux
%attr(4755,root,root)	/bin/ping
/sbin/arping
%{_sbindir}/arping
%else
%{_sbindir}/arping
%attr(4755,root,root)	%{_sbindir}/ping
%endif
%attr(4755,root,root) %{_sbindir}/ping6
%{_sbindir}/tracepath
%{_sbindir}/tracepath6
%attr(4755,root,root) %{_sbindir}/traceroute6
%{_sbindir}/rdisc
%{_mandir}/man8/*


%changelog
* Sat Feb 23 2001 Alexey Kuznetsov <kuznet@ms2.inr.ac.ru>
  Taken iputils rpm from ASPLinux-7.2 as pattern.
