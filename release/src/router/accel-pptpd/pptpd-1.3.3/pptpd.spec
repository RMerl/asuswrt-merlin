%{!?__id_u: %define __id_u %([ -x /bin/id ]&&echo /bin/id||([ -x /usr/bin/id ]&&echo /usr/bin/id|| echo /bin/true)) -u}

# Available rpmbuild options:
#
# --without libwrap
# --with    bsdppp
# --with    slirp
# --with    ipalloc
# --without bcrelay
#

Summary:        PoPToP Point to Point Tunneling Server
Name:           pptpd
Version:        1.3.3
Release:        1
License:        GPL
Group:          Applications/Internet
URL:            http://poptop.sourceforge.net/
Source0:        http://dl.sf.net/poptop/pptpd-%{version}.tar.gz
Buildroot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires:       ppp >= 2.4.3

%if %{?_without_libwrap:0}%{!?_without_libwrap:1}
BuildRequires: tcp_wrappers
%endif

Requires(post):  /sbin/chkconfig
Requires(preun): /sbin/chkconfig, /sbin/service

%description
This implements a Virtual Private Networking Server (VPN) that is
compatible with Microsoft VPN clients. It allows windows users to
connect to an internal firewalled network using their dialup.

%prep
%setup -q

# Fix permissions for debuginfo package
%{__chmod} 644 *.[ch]

# Fix multilib
%{__perl} -pi -e 's,/usr/lib/pptpd,%{_libdir}/pptpd,;' pptpctrl.c

%build
%configure \
	%{!?_without_libwrap:--with-libwrap} \
	%{?_without_libwrap:--without-libwrap} \
	%{!?_with_bsdppp:--without-bsdppp} \
	%{?_with_bsdppp:--with-bsdppp} \
	%{!?_with_slirp:--without-slirp} \
	%{?_with_slirp:--with-slirp} \
	%{!?_with_ipalloc:--without-pppd-ip-alloc} \
	%{?_with_ipalloc:--with-pppd-ip-alloc} \
	%{!?_without_bcrelay:--with-bcrelay} \
	%{?_without_bcrelay:--without-bcrelay}
(echo '#undef VERSION'; echo '#define VERSION "2.4.3"') >> plugins/patchlevel.h
%{__make} CFLAGS='-fno-builtin -fPIC -DSBINDIR=\"%{_sbindir}\" %{optflags}'

%install
%{__rm} -rf %{buildroot}
%{__mkdir_p} %{buildroot}/etc/rc.d/init.d
%{__mkdir_p} %{buildroot}/etc/ppp
%{__mkdir_p} %{buildroot}%{_bindir}
%{__mkdir_p} %{buildroot}%{_mandir}/man{5,8}
%{__make} \
	DESTDIR=%{buildroot} \
	INSTALL=%{__install} \
	LIBDIR=%{buildroot}%{_libdir}/pptpd \
	install
%{__install} -m 0755 pptpd.init %{buildroot}/etc/rc.d/init.d/pptpd
%{__install} -m 0644 samples/pptpd.conf %{buildroot}/etc/pptpd.conf
%{__install} -m 0644 samples/options.pptpd %{buildroot}/etc/ppp/options.pptpd
%{__install} -m 0755 tools/vpnuser %{buildroot}%{_bindir}/vpnuser
%{__install} -m 0755 tools/vpnstats.pl %{buildroot}%{_bindir}/vpnstats.pl
%{__install} -m 0755 tools/pptp-portslave %{buildroot}%{_sbindir}/pptp-portslave
%{__install} -m 0644 pptpd.conf.5 %{buildroot}%{_mandir}/man5/pptpd.conf.5
%{__install} -m 0644 pptpd.8 %{buildroot}%{_mandir}/man8/pptpd.8
%{__install} -m 0644 pptpctrl.8 %{buildroot}%{_mandir}/man8/pptpctrl.8

%post
/sbin/chkconfig --add pptpd || :
OUTD="" ; for i in d manager ctrl ; do
    test -x /sbin/pptp$i && OUTD="$OUTD /sbin/pptp$i" ;
done
test -z "$OUTD" || \
{ echo "possible outdated executable detected; we now use %{_sbindir}/pptp*, perhaps you should run the following command:"; echo "rm -i $OUTD" ;}

%postun
[ $1 -gt 0 ] && /sbin/service pptpd condrestart &> /dev/null || :

%preun
if [ "$1" -lt 1 ]; then
    /sbin/service pptpd stop &> /dev/null || :
    /sbin/chkconfig --del pptpd || :
fi

%clean
%{__rm} -rf %{buildroot}

%files
%defattr(-,root,root,0755)
%doc AUTHORS COPYING INSTALL README* TODO ChangeLog* samples
%{_sbindir}/pptpd
%{_sbindir}/pptpctrl
%{_sbindir}/pptp-portslave
%{!?_without_bcrelay:%{_sbindir}/bcrelay}
%{_libdir}/pptpd/pptpd-logwtmp.so
%{_bindir}/vpnuser
%{_bindir}/vpnstats.pl
%{_mandir}/man5/pptpd.conf.5*
%{_mandir}/man8/pptpd.8*
%{_mandir}/man8/pptpctrl.8*
/etc/rc.d/init.d/pptpd
%config(noreplace) /etc/pptpd.conf
%config(noreplace) /etc/ppp/options.pptpd

%changelog
* Fri Mar 31 2006 Paul Howarth <paul@city-fan.org> - 1.3.1-1
- Update to 1.3.1

* Fri Mar 31 2006 Paul Howarth <paul@city-fan.org> - 1.3.0-1
- update to 1.3.0
- remove redundant macro definitions
- change Group: to one listed in rpm's GROUPS file
- use full URL for source
- simplify conditional build code
- use macros for destination directories
- honour %%{optflags}
- general spec file cleanup
- initscript updates:
    don't enable the service by default
    add reload and condrestart options
- condrestart service on package upgrade
- fix build on x86_64
- add buildreq tcp_wrappers

* Fri Feb 18 2005 James Cameron <james.cameron@hp.com>
- fix to use ppp 2.4.3 for plugin

* Thu Nov 11 2004 James Cameron <james.cameron@hp.com>
- adjust for building on Red Hat Enterprise Linux, per Charlie Brady
- remove vpnstats, superceded by vpnstats.pl

* Fri May 21 2004 James Cameron <james.cameron@hp.com>
- adjust for packaging naming and test

* Fri Apr 23 2004 James Cameron <james.cameron@hp.com>
- include vpnwho.pl

* Thu Apr 22 2004 James Cameron <james.cameron@hp.com>
- change description wording
- change URL for upstream
- release first candidate for 1.2.0

* Fri Jul 18 2003 R. de Vroede <richard@oip.tudelft.nl>
- Check the ChangeLog files.

