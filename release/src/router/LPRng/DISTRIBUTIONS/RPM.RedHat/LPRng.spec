Summary: The LPRng print spooler.
Name: LPRng
Version: 3.7.6
Release: 1
License: GPL and Artistic
Group: System Environment/Daemons
Source0: ftp://ftp.lprng.com/pub/LPRng/LPRng/%{name}-%{version}.tgz
Source1: lpd.init
URL: http://www.lprng.com
Buildroot: %{_tmppath}/%{name}-%{version}-%{release}-root
Obsoletes: lpr
Provides: lpr
Prereq: /sbin/chkconfig, /etc/rc.d/init.d, mktemp, fileutils, textutils, gawk
BuildPreReq: gettext
%{!?nokerberos:BuildPrereq: krb5-devel}
%{!?nokerberos:Requires: krb5-libs}

%description
LPRng is an enhanced, extended, and portable implementation of the
Berkeley LPR print spooler functionality. LPRng provides the same
interface and meeting RFC1179 requirements, but the implementation is
completely new and provides support for the following features:
lightweight (no databases needed) lpr, lpc, and lprm programs; dynamic
redirection of print queues; automatic job holding; highly verbose
diagnostics; multiple printers serving a single queue; client programs
do not need to run SUID root; greatly enhanced security checks; and a
greatly improved permission and authorization mechanism.

LPRng is compatible with other print spoolers and network printers
that use the LPR interface and meet RFC1179 requirements. LPRng
provides emulation packages for the SVR4 lp and lpstat programs,
eliminating the need for another print spooler package. These
emulation packages can be modified according to local requirements, in
order to support vintage printing systems.

For users who require secure and/or authenticated printing support,
LPRng supports Kerberos V, MIT Kerberos IV Print Support, and PGP
authentication. Additional authentication support is extremely simple
to add.

%prep

%setup -q

# pick up configure.in changes

# set up gettext
(
cd po
rm Makefile.in.in
ln -s /usr/share/gettext/po/Makefile.in.in .
)

#sh CREATE_CONFIGURE
    
%build
CFLAGS="$RPM_OPT_FLAGS" ; export CFLAGS
%configure --enable-nls \
	--with-userid=lp \
	--with-groupid=lp \
%{!?nokerberos:--enable-kerberos LDFLAGS="-L/usr/kerberos/lib" CPPFLAGS="-I/usr/kerberos/include" }
make MAKEPACKAGE=YES

%install
rm -rf %{buildroot}

# Installation of locales is broken... Work around it!
#perl -pi -e "s,prefix =.*,prefix = ${RPM_BUILD_ROOT}%{_prefix},g" po/Makefile
#perl -pi -e "s,datadir =.*,datadir = ${RPM_BUILD_ROOT}%{_prefix}/share,g" po/Makefile
#perl -pi -e "s,localedir =.*,localedir = ${RPM_BUILD_ROOT}%{_prefix}/share/locale,g" po/Makefile
#perl -pi -e "s,gettextsrcdir =.*,gettextsrcdir = ${RPM_BUILD_ROOT}%{_prefix}/share/gettext/po,g" po/Makefile

make SUID_ROOT_PERMS=" 04755" DESTDIR=${RPM_BUILD_ROOT} MAKEPACKAGE=YES mandir=%{_mandir} install
%__cp src/monitor ${RPM_BUILD_ROOT}%{_prefix}/sbin/monitor

# install init script
#mkdir -p %{buildroot}%{_sysconfdir}/rc.d/init.d
#install -m755 %{SOURCE1} %{buildroot}%{_sysconfdir}/rc.d/init.d/lpd
mkdir -p ${RPM_BUILD_ROOT}%{_sysconfdir}/rc.d/init.d
install -m755 %{SOURCE1} ${RPM_BUILD_ROOT}%{_sysconfdir}/rc.d/init.d/lpd

%clean
rm -rf %{buildroot}

%post
/sbin/chkconfig --add lpd
if [ -w /etc/printcap ] ; then
  TMP1=`mktemp /etc/printcap.XXXXXX`
  gawk '
    BEGIN { first = 1; cont = 0; last = "" }
    /^[:space:]*#/      { if(cont) sub("\\\\$", "", last)}
    { if(first == 0) print last }
    { first = 0 }
    { last = $0 }
    { cont = 0 }
    /\\$/ { cont = 1 }
    END {sub("\\\\$", "", last); print last}
  ' /etc/printcap > ${TMP1} && cat ${TMP1} > /etc/printcap && rm -f ${TMP1}
fi

%preun
if [ "$1" = 0 ]; then
  %{_sysconfdir}/rc.d/init.d/lpd stop >/dev/null 2>&1
  /sbin/chkconfig --del lpd
fi

%postun
if [ "$1" -ge "1" ]; then
  %{_sysconfdir}/rc.d/init.d/lpd condrestart >/dev/null 2>&1
fi

%files
%defattr(-,root,root)
%config %{_sysconfdir}/lpd/lpd.conf
%config %{_sysconfdir}/lpd/lpd.perms
%config %{_sysconfdir}/printcap
%attr(644,root,root) %{_sysconfdir}/lpd/lpd.conf.sample
%attr(644,root,root) %{_sysconfdir}/lpd/lpd.perms.sample
%attr(644,root,root) %{_sysconfdir}/printcap.sample
%attr(755,root,root) %{_libdir}/*
%config %{_sysconfdir}/rc.d/init.d/lpd
%attr(755,lp,lp) %{_bindir}/lpq
%attr(755,lp,lp) %{_bindir}/lprm
%attr(755,lp,lp) %{_bindir}/lpr
%attr(755,lp,lp) %{_bindir}/lpstat
%{_bindir}/lp
%{_bindir}/cancel
%attr(755,lp,lp) %{_sbindir}/lpc
%attr(755,root,root)  %{_sbindir}/lpd
%attr(755,root,root)  %{_sbindir}/lprng_certs
%attr(755,root,root)  %{_sbindir}/lprng_index_certs
%attr(755,root,root)  %{_sbindir}/checkpc
%attr(755,root,root)  %{_sbindir}/monitor
%attr(755,root,root)  /usr/libexec/filters/*
%dir /usr/libexec/filters
%{_mandir}/*/*
%attr(755,root,root) %{_prefix}/share/locale/*
%doc CHANGES CONTRIBUTORS COPYRIGHT INSTALL LICENSE 
%doc README* VERSION Y2KCompliance
%doc DOCS/*.html DOCS/*.jpg DOCS/*.pdf PrintingCookbook/HTML/* PrintingCookbook/PDF/*

%changelog
* Tue Aug 21 2001 Patrick Powell <papowell@astart.com> 3.7.5-1
- new release for 3.7.5
- Note the aclocal, autoconf, etc. stuff added to make the various
- versions of gettext and libtool compatible with the RedHat version.

* Fri Aug 10 2001 Crutcher Dunnavant <crutcher@redhat.com> 3.7.4-28
- disabled gdbm support, changed CFLAGS; #41652

* Thu Aug  9 2001 Crutcher Dunnavant <crutcher@redhat.com> 3.7.4-27
- ownz /usr/libexec/filters; #51158
- make checkpc nonblocking on its tests; #37995

* Thu Aug  2 2001 Crutcher Dunnavant <crutcher@redhat.com> 3.7.4-26
- added gdbm-devel dep; #44885

* Fri Jun 29 2001 Bernhard Rosenkraenzer <bero@redhat.com>
- Fix build on s390

* Sun Jun 24 2001 Elliot Lee <sopwith@redhat.com>
- Bump release + rebuild.

* Thu Jun 07 2001 Crutcher Dunnavant <crutcher@redhat.com>
- setgroups droping patch

* Thu Mar 29 2001 Crutcher Dunnavant <crutcher@redhat.com>
- told checkpc to only Check_file() job files. (keeping it from zapping the master-filter)

* Fri Mar  9 2001 Crutcher Dunnavant <crutcher@redhat.com>
- applied elliot's patch (which i got idependently from the lprng list as well)
- that fixes a thinko that killed LPRng on 64 arches.

* Fri Mar  9 2001 Crutcher Dunnavant <crutcher@redhat.com>
- fixed the URLs for the LPRng project

* Thu Mar  8 2001 Crutcher Dunnavant <crutcher@redhat.com>
- reverted the shutdown call in Local_job to make device filters work.
- all your base are belong to us

* Wed Mar 07 2001 Philipp Knirsch <pknirsch@redhat.de>
- Removed the shutdown patch for common/linksupport:Link_close() as ii
generated a deadlock between lpr and lpd for none existing printers.

* Sun Feb 25 2001 Crutcher Dunnavant <crutcher@redhat.com>
- hacked out the shutdown(sock, 1) calls that killed stupid network printers.

* Wed Feb 15 2001 Crutcher Dunnavant <crutcher@redhat.com>
- tweak to ldp.init's display messages

* Fri Feb  9 2001 Crutcher Dunnavant <crutcher@redhat.com>
- man page tweak

* Thu Feb  8 2001 Crutcher Dunnavant <crutcher@redhat.com>
- (yet) further tweaks to lpd.init's display format.

* Tue Feb  6 2001 Crutcher Dunnavant <crutcher@redhat.com>
- further tweaks to lpd.init's display format.

* Tue Feb  6 2001 Crutcher Dunnavant <crutcher@redhat.com>
- cleaned up lpd.init to do translations correctly, and stop screwing up
- result values.

* Thu Feb  1 2001 Crutcher Dunnavant <crutcher@redhat.com>
- added lpd init back (ick!)

* Wed Jan 24 2001 Crutcher Dunnavant <crutcher@redhat.com>
- removed req for printconf (It should go the other way)

* Thu Jan 18 2001 Crutcher Dunnavant <crutcher@redhat.com>
- fixed file list

* Thu Jan 18 2001 Crutcher Dunnavant <crutcher@redhat.com>
- removed lpd.init, set requirement of printconf

* Thu Jan 11 2001 Crutcher Dunnavant <crutcher@redhat.com>
- upgraded to LPRng-3.7.4

* Tue Dec 12 2000 Crutcher Dunnavant <crutcher@redhat.com>
- rebuild for kerb

* Mon Oct 23 2000 Crutcher Dunnavant <crutcher@redhat.com>
- Upgraded to LPRng-3.6.26
- Removed syslog patch, as .24 fixes the problem

* Mon Sep 25 2000 Crutcher Dunnavant <crutcher@redhat.com>
- Patched use_syslog to avoid a format string exploit.
- Resolves bug #17756

* Mon Sep 18 2000 Crutcher Dunnavant <crutcher@redhat.com>
- Upgraded to LPRng-3.6.24

* Mon Sep 11 2000 Crutcher Dunnavant <crutcher@redhat.com>
- Added gettext to the BuildPreReq list

* Mon Sep 11 2000 Crutcher Dunnavant <crutcher@redhat.com>
- Fixed lpd.init to use /etc/rc.d/init.d/, instead of /etc/init.d

* Mon Sep 11 2000 Crutcher Dunnavant <crutcher@redhat.com>
- changed the prereq: /etc/init.d to: /etc/rc.d/init.d
- we are not changing that over (yet?)

* Mon Aug 14 2000 Crutcher Dunnavant <crutcher@redhat.com>
- removed the sticky bit from lpc

* Mon Aug 14 2000 Crutcher Dunnavant <crutcher@redhat.com>
- removed the sticky bit from the client programs (LPRng doesn't need them)

* Sat Aug 05 2000 Bill Nottingham <notting@redhat.com>
- condrestart fixes

* Fri Aug  4 2000 Bill Nottingham <notting@redhat.com>
- triggerpostun on lpr

* Sun Jul 30 2000 Bernhard Rosenkraenzer <bero@redhat.com>
- 3.6.22 (some fixes)

* Tue Jul 25 2000 Bill Nottingham <notting@redhat.com>
- fix prereq

* Sat Jul 22 2000 Nalin Dahyabhai <nalin@redhat.com>
- fix bogus checkpc error messages when the lockfile doesn't exist because
  init scripts clear /var/run (#14472)

* Tue Jul 18 2000 Nalin Dahyabhai <nalin@redhat.com>
- fix chkconfig comments in the init script

* Mon Jul 17 2000 Nalin Dahyabhai <nalin@redhat.com>
- move the init script to /etc/rc.d/init.d
- fix perms on setuid binaries

* Fri Jul 14 2000 Nalin Dahyabhai <nalin@redhat.com>
- patch checkpc to not complain when filter is executable and in the
  spool directory
- remove --disable-force_localhost from configure invocation for better
  compatibility with BSD LPR and rhs-printfilters
- change group back to lp, which is what printtool expects

* Thu Jul 13 2000 Nalin Dahyabhai <nalin@redhat.com>
- change default group to 'daemon' to match 6.2
- enable NLS support
- remove Prefix: tag
- break init script out into a separate file
- fix up broken printcaps in post-install
- run checkpc -f at start-time

* Wed Jul 12 2000 Prospector <bugzilla@redhat.com>
- automatic rebuild

* Tue Jul 11 2000 Bernhard Rosenkraenzer <bero@redhat.com>
- 3.6.21
- get rid of the notypedef patch - gcc has been fixed at last.

* Mon Jun 26 2000 Preston Brown <pbrown@redhat.com>
- sample config files removed from /etc.
- initscript moved to /etc/init.d

* Wed Jun 21 2000 Bernhard Rosenkraenzer <bero@redhat.com>
- 3.6.18

* Sat Jun 17 2000 Bernhard Rosenkraenzer <bero@redhat.com>
- 3.6.16
- adapt Kerberos and fixmake patches
- get rid of CFLAGS="-O"; gcc has been fixed
- fix build with glibc 2.2

* Mon Jun 05 2000 Preston Brown <pbrown@redhat.com>
- ifdef 0 is illegal, changed to if 0.
- work around compiler typdef bug.

* Thu Jun 01 2000 Preston Brown <pbrown@redhat.com>
- start, stop, and restart are functions not switch statements now.
  reduces overhead.
- patch to allow autoconf to choose which user/group to run as

* Wed May 31 2000 Preston Brown <pbrown@redhat.com>
- remove init.d symbolic links.
- remove txt/ps/info versions of the HOWTO from the pkg
- use new fhs paths

* Thu May 25 2000 Nalin Dahyabhai <nalin@redhat.com>
- change free() to krb5_free_data_contents() when patching for Kerberos 5
- detect libcrypto or libk5crypto when looking for Kerberos 5

* Tue May 16 2000 Nalin Dahyabhai <nalin@redhat.com>
- enable Kerberos support
- remove extra defattr in files list
- add --disable-force_localhost to configure invocation
- remove "-o root" at install-time

* Tue May 16 2000 Matt Wilson <msw@redhat.com>
- add Prereq of /sbin/chkconfig
- fix broken conflicting declaration on alpha

* Tue Apr 18 2000 Bernhard Rosenkraenzer <bero@redhat.com>
- initial Red Hat packaging, fix up the spec file

* Mon Sep 13 1999 Patrick Powell <papowell@astart.com>
- resolved problems with symbolic links to /etc/init.d
  files - used the chkconfig facility

* Sat Sep  4 1999 Patrick Powell <papowell@astart.com>
- did ugly things to put the script in the spec file

* Sat Aug 28 1999 Giulio Orsero <giulioo@tiscalinet.it>
- 3.6.8

* Fri Aug 27 1999 Giulio Orsero <giulioo@tiscalinet.it>
- 3.6.7 First RPM build.
