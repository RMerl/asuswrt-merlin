Summary: Layer 2 Tunnelling Protocol Daemon (RFC 2661)
Name: xl2tpd
Version: 1.2.6
Release: 1%{?dist}
License: GPLv2
Url: http://www.xelerance.com/software/xl2tpd/
Group: System Environment/Daemons
Source0: http://www.xelerance.com/software/xl2tpd/xl2tpd-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires: ppp >= 2.4.3
%if %{sles_version} == 0
BuildRequires: linux-kernel-headers => 2.6.19, libpcap-devel
%else
BuildRequires: glibc-devel, libpcap
%endif
Obsoletes: l2tpd <= 0.69
Provides: l2tpd = 0.69
Requires(post): /sbin/chkconfig
Requires(preun): /sbin/chkconfig
Requires(preun): /sbin/service

%description
xl2tpd is an implementation of the Layer 2 Tunnelling Protocol (RFC 2661).
L2TP allows you to tunnel PPP over UDP. Some ISPs use L2TP to tunnel user
sessions from dial-in servers (modem banks, ADSL DSLAMs) to back-end PPP
servers. Another important application is Virtual Private Networks where
the IPsec protocol is used to secure the L2TP connection (L2TP/IPsec,
RFC 3193). The L2TP/IPsec protocol is mainly used by Windows and
Mac OS X clients. On Linux, xl2tpd can be used in combination with IPsec
implementations such as Openswan.
Example configuration files for such a setup are included in this RPM.

xl2tpd works by opening a pseudo-tty for communicating with pppd.
It runs completely in userspace but supports kernel mode L2TP.

xl2tpd supports IPsec SA Reference tracking to enable overlapping internak
NAT'ed IP's by different clients (eg all clients connecting from their
linksys internal IP 192.168.1.101) as well as multiple clients behind
the same NAT router.

xl2tpd supports the pppol2tp kernel mode operations on 2.6.23 or higher,
or via a patch in contrib for 2.4.x kernels.

Xl2tpd is based on the 0.69 L2TP by Jeff McAdams <jeffm@iglou.com>
It was de-facto maintained by Jacco de Leeuw <jacco2@dds.nl> in 2002 and 2003.

%prep
%setup -q

%build
make DFLAGS="$RPM_OPT_FLAGS -g -DDEBUG_PPPD -DDEBUG_CONTROL -DDEBUG_ENTROPY -DTRUST_PPPD_TO_DIE"

%install
rm -rf %{buildroot}
make PREFIX=%{_prefix} DESTDIR=%{buildroot} MANDIR=%{buildroot}/%{_mandir} install
install -p -D -m644 examples/xl2tpd.conf %{buildroot}%{_sysconfdir}/xl2tpd/xl2tpd.conf
install -p -D -m644 examples/ppp-options.xl2tpd %{buildroot}%{_sysconfdir}/ppp/options.xl2tpd
install -p -D -m600 doc/l2tp-secrets.sample %{buildroot}%{_sysconfdir}/xl2tpd/l2tp-secrets
install -p -D -m600 examples/chapsecrets.sample %{buildroot}%{_sysconfdir}/ppp/chap-secrets.sample
install -p -D -m755 packaging/suse/xl2tpd.init %{buildroot}%{_initrddir}/xl2tpd
ln -sf /etc/init.d/xl2tpd $RPM_BUILD_ROOT/usr/sbin/rcxl2tpd
install -p -D -m755 -d %{buildroot}%{_localstatedir}/run/xl2tpd


%clean
rm -rf %{buildroot}

%post
%{fillup_and_insserv xl2tpd}

/sbin/chkconfig --add xl2tpd
# if we migrate from l2tpd to xl2tpd, copy the configs
if [ -f /etc/l2tpd/l2tpd.conf ]
then
	echo "Old /etc/l2tpd configuration found, migrating to /etc/xl2tpd"
	mv /etc/xl2tpd/xl2tpd.conf /etc/xl2tpd/xl2tpd.conf.rpmsave
	cat /etc/l2tpd/l2tpd.conf | sed "s/options.l2tpd/options.xl2tpd/" > /etc/xl2tpd/xl2tpd.conf
	mv /etc/ppp/options.xl2tpd /etc/ppp/options.xl2tpd.rpmsave
	mv /etc/ppp/options.l2tpd /etc/ppp/options.xl2tpd
	mv /etc/xl2tpd/l2tp-secrets /etc/xl2tpd/l2tpd-secrets.rpmsave
	cp -pa /etc/l2tpd/l2tp-secrets /etc/xl2tpd/l2tp-secrets
	
fi


%preun
%stop_on_removal xl2tpd
exit 0

%postun
%restart_on_update xl2tpd
%insserv_cleanup
exit 0

%files
%defattr(-,root,root)
%doc BUGS CHANGES CREDITS LICENSE README.* TODO doc/rfc2661.txt 
%doc doc/README.patents examples/chapsecrets.sample
%{_sbindir}/rcxl2tpd
%{_sbindir}/xl2tpd
%{_bindir}/pfc
%{_mandir}/*/*
%dir %{_sysconfdir}/xl2tpd
%config(noreplace) %{_sysconfdir}/xl2tpd/*
%config(noreplace) %{_sysconfdir}/ppp/*
%attr(0755,root,root)  %{_initrddir}/xl2tpd
%dir %{_localstatedir}/run/xl2tpd

%changelog
* Sun Oct 26 2008 Paul Wouters <paul@xelerance.com> 1.2.2-1
- Updated Suse init scripts and spec file
- Added pfc for pppd's precompiled-active-filter

* Fri Apr 18 2008 Paul Wouters <paul@xelerance.com> 1.2.1-1
- Updated Suse init scripts and spec file

* Tue Jun 26 2007 Paul Wouters <paul@xelerance.com> 1.1.11-1
- Minor changes to spec file to accomodate new README files

* Fri Feb 23 2007 Paul Wouters <paul@xelerance.com> 1.1.08-1
- Upgraded to 1.1.08
- This works around the ppp-2.4.2-6.4 issue of not dying on SIGTERM

* Mon Feb 19 2007 Paul Wouters <paul@xelerance.com> 1.1.07-2
- Upgraded to 1.1.07
- Fixes from Tuomo Soini for pidfile handling with Fedora
- Fix hardcoded version for Source in spec file.

* Thu Dec  7 2006 Paul Wouters <paul@xelerance.com> 1.1.06-5
- Changed space/tab replacing method

* Wed Dec  6 2006 Paul Wouters <paul@xelerance.com> 1.1.06-4
- Added -p to keep original timestamps
- Added temporary hack to change space/tab in init file.
- Added /sbin/service dependancy

* Tue Dec  5 2006 Paul Wouters <paul@xelerance.com> 1.1.06-3
- Added Requires(post) / Requires(preun)
- changed init file to create /var/run/xl2tpd fixed a tab/space
- changed control file to be within /var/run/xl2tpd/

* Tue Dec  5 2006 Paul Wouters <paul@xelerance.com> 1.1.06-2
- Changed Mr. Karlsen's name to not be a utf8 problem
- Fixed Obosoletes/Provides to be more specific wrt l2tpd.
- Added dist tag which accidentally got deleted.

* Mon Dec  4 2006 Paul Wouters <paul@xelerance.com> 1.1.06-1
- Rebased spec file on Fedora Extras copy, but using xl2tpd as package name

* Sun Nov 27 2005 Paul Wouters <paul@xelerance.com> 0.69.20051030
- Pulled up sourceforget.net CVS fixes.
- various debugging added, but debugging should not be on by default.
- async/sync conversion routines must be ready for possibility that the read
  will block due to routing loops.
- refactor control socket handling.
- move all logic about pty usage to pty.c. Try ptmx first, if it fails try
  legacy ptys
- rename log() to l2tp_log(), as "log" is a math function.
- if we aren't deamonized, then log to stderr.
- added install: and DESTDIR support.

* Thu Oct 20 2005 Paul Wouters <paul@xelerance.com> 0.69-13
- Removed suse/mandrake specifics. Comply for Fedora Extras guidelines

* Tue Jun 21 2005 Jacco de Leeuw <jacco2@dds.nl> 0.69-12jdl
- Added log() patch by Paul Wouters so that l2tpd compiles on FC4.

* Sat Jun 4 2005 Jacco de Leeuw <jacco2@dds.nl>
- l2tpd.org has been hijacked. Project moved back to SourceForge:
  http://l2tpd.sourceforge.net 

* Tue May 3 2005 Jacco de Leeuw <jacco2@dds.nl>
- Small Makefile fixes. Explicitly use gcc instead of cc. 
  Network services library was not linked on Solaris due to typo.

* Thu Mar 17 2005 Jacco de Leeuw <jacco2@dds.nl> 0.69-11jdl
- Choosing between SysV or BSD style ptys is now configurable through
  a compile-time boolean "unix98pty".

* Fri Feb 4 2005 Jacco de Leeuw <jacco2@dds.nl>
- Added code from Roaring Penguin (rp-l2tp) to support SysV-style ptys.
  Requires the N_HDLC kernel module. 

* Fri Nov 26 2004 Jacco de Leeuw <jacco2@dds.nl>
- Updated the README.

* Wed Nov 10 2004 Jacco de Leeuw <jacco2@dds.nl> 0.69-10jdl
- Patch by Marald Klein and Roger Luethi. Fixes writing PID file.
  (http://l2tpd.graffl.net/msg01790.html)
  Long overdue. Rereleasing 10jdl.

* Tue Nov 9 2004 Jacco de Leeuw <jacco2@dds.nl> 0.69-10jdl
- [SECURITY FIX] Added fix from Debian because of a bss-based
  buffer overflow.
  (http://www.mail-archive.com/l2tpd-devel@l2tpd.org/msg01071.html)
- Mandrake's FreeS/WAN, Openswan and Strongswan RPMS use configuration
  directories /etc/{freeswan,openswan,strongswan}. Install our
  configuration files to /etc/ipsec.d and create symbolic links in
  those directories.

* Tue Aug 18 2004 Jacco de Leeuw <jacco2@dds.nl>
- Removed 'leftnexthop=' lines. Not relevant for recent versions
  of FreeS/WAN and derivates.

* Tue Jan 20 2004 Jacco de Leeuw <jacco2@dds.nl>  0.69-9jdl
- Added "noccp" because of too much MPPE/CCP messages sometimes.

* Wed Dec 31 2003 Jacco de Leeuw <jacco2@dds.nl>
- Added patch in order to prevent StopCCN messages.

* Sat Aug 23 2003 Jacco de Leeuw <jacco2@dds.nl>
- MTU/MRU 1410 seems to be the lowest possible for MSL2TP.
  For Windows 2000/XP it doesn't seem to matter.
- Typo in l2tpd.conf (192.168.128/25).

* Fri Aug 8 2003 Jacco de Leeuw <jacco2@dds.nl>  0.69-8jdl
- Added MTU/MRU 1400 to options.l2tpd. I don't know the optimal
  value but some apps had problems with the default value.

* Fri Aug 1 2003 Jacco de Leeuw <jacco2@dds.nl>
- Added workaround for the missing hostname bug in the MSL2TP client
  ('Specify your hostname', error 629: "You have been disconnected
  from the computer you are dialing").

* Thu Jul 20 2003 Jacco de Leeuw <jacco2@dds.nl>  0.69-7jdl
- Added the "listen-addr" global parameter for l2tpd.conf. By
  default, the daemon listens on *all* interfaces. Use
  "listen-addr" if you want it to bind to one specific
  IP address (interface), for security reasons. (See also:
  http://www.jacco2.dds.nl/networking/freeswan-l2tp.html#Firewallwarning)
- Explained in l2tpd.conf that two different IP addresses should be
  used for 'listen-addr' and 'local ip'.
- Modified init script. Upgrades should work better now. You
  still need to start/chkconfig l2tpd manually.
- Renamed the example Openswan .conf files to better reflect
  the situation. There are two variants using different portselectors.
  Previously I thought Windows 2000/XP used portselector 17/0
  and the rest used 17/1701. But with the release of an updated 
  IPsec client by Microsoft, it turns out that 17/0 must have
  been a mistake: the updated client now also uses 17/1701.

* Mon Apr 10 2003 Jacco de Leeuw <jacco2@dds.nl>  0.69-6jdl
- Changed sample chap-secrets to be valid only for specific
  IP addresses.

* Thu Mar 13 2003 Bernhard Thoni <tech-role@tronicplanet.de>
- Adjustments for SuSE8.x (thanks, Bernhard!)
- Added sample chap-secrets.

* Thu Mar 6 2003 Jacco de Leeuw <jacco2@dds.nl> 0.69-5jdl
- Replaced Dominique's patch by Damion de Soto's, which does not
  depend on the N_HDLC kernel module. 

* Wed Feb 26 2003 Jacco de Leeuw <jacco2@dds.nl> 0.69-4jdl
- Seperate example config files for Win9x (MSL2TP) and Win2K/XP
  due to left/rightprotoport differences.
  Fixing preun for Red Hat.

* Mon Feb 3 2003 Jacco de Leeuw <jacco2@dds.nl> 0.69-3jdl
- Mandrake uses /etc/freeswan/ instead of /etc/ipsec.d/
  Error fixed: source6 was used for both PSK and CERT.

* Wed Jan 29 2003 Jacco de Leeuw <jacco2@dds.nl> 0.69-3jdl
- Added Dominique Cressatti's pty patch in another attempt to
  prevent the Windows 2000 Professional "loopback detected" error.
  Seems to work!

* Wed Dec 25 2002 Jacco de Leeuw <jacco2@dds.nl> 0.69-2jdl
- Added 'connect-delay' to PPP parameters in an attempt to
  prevent the Windows 2000 Professional "loopback detected" error.
  Didn't seem to work.

* Fri Dec 13 2002 Jacco de Leeuw <jacco2@dds.nl> 0.69-1jdl
- Did not build on Red Hat 8.0. Solved by adding comments(?!).
  Bug detected in spec file: chkconfig --list l2tpd does not work
  on Red Hat 8.0. Not important enough to look into yet.

* Sun Nov 17 2002 Jacco de Leeuw <jacco2@dds.nl> 0.69-1jdl
- Tested on Red Hat, required some changes. No gprintf. Used different
  pty patch, otherwise wouldn't run. Added buildroot sanity check.

* Sun Nov 10 2002 Jacco de Leeuw <jacco2@dds.nl>
- Specfile adapted from Mandrake Cooker. The original RPM can be
  retrieved through:
  http://www.rpmfind.net/linux/rpm2html/search.php?query=l2tpd
- Config path changed from /etc/l2tp/ to /etc/l2tpd/ 
  (Seems more logical and rp-l2tp already uses /etc/l2tp/).
- Do not run at boot or install. The original RPM uses a config file
  which is completely commented out, but it still starts l2tpd on all
  interfaces. Could be a security risk. This RPM does not start l2tpd,
  the sysadmin has to edit the config file and start l2tpd explicitly.
- Renamed patches to start with l2tpd-
- Added dependencies for pppd, glibc-devel.
- Use %%{name} as much as possible.
- l2tp-secrets contains passwords, thus should not be world readable.
- Removed dependency on rpm-helper.

* Mon Oct 21 2002 Lenny Cartier <lenny@mandrakesoft.com> 0.69-3mdk
- from Per 0yvind Karlsen <peroyvind@delonic.no> :
 - PreReq and Requires
 - Fix preun_service

* Thu Oct 17 2002 Per 0yvind Karlsen <peroyvind@delonic.no> 0.69-2mdk
- Move l2tpd from /usr/bin to /usr/sbin
- Added SysV initscript
- Patch0
- Patch1

* Thu Oct 17 2002 Per 0yvind Karlsen <peroyvind@delonic.no> 0.69-1mdk
- Initial release
