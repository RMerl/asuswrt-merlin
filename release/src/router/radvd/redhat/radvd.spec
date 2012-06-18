
%define initdir %{_sysconfdir}/rc.d/init.d

%define RADVD_UID 75

Summary: A Router Advertisement daemon
Name: radvd
Version: 1.8.5
Release: 1
# The code includes the advertising clause, so it's GPL-incompatible
License: BSD with advertising
Group: System Environment/Daemons
URL:        http://www.litech.org/radvd/
Source:     http://www.litech.org/radvd/dist/%{name}-%{version}.tar.gz
Requires(postun):   chkconfig, initscripts
Requires(preun):    chkconfig, initscripts
Requires(post):     chkconfig
Requires(pre):      /usr/sbin/useradd
BuildRequires: flex, byacc
BuildRoot:          %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
radvd is the router advertisement daemon for IPv6.  It listens to router
solicitations and sends router advertisements as described in "Neighbor
Discovery for IP Version 6 (IPv6)" (RFC 2461).  With these advertisements
hosts can automatically configure their addresses and some other
parameters.  They also can choose a default router based on these
advertisements.

Install radvd if you are setting up IPv6 network and/or Mobile IPv6
services.

%prep
%setup -q

%build
export CFLAGS="$RPM_OPT_FLAGS -D_GNU_SOURCE -fPIE" 
export LDFLAGS='-pie -Wl,-z,relro,-z,now,-z,noexecstack,-z,nodlopen'
%configure --with-pidfile=%{_localstatedir}/run/radvd/radvd.pid
make
# make %{?_smp_mflags} 
# Parallel builds still fail because seds that transform y.tab.x into
# scanner/gram.x are not executed before compile of scanner/gram.x
#

%install
[ $RPM_BUILD_ROOT != "/" ] && rm -rf $RPM_BUILD_ROOT

make DESTDIR=$RPM_BUILD_ROOT install

mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig
mkdir -p $RPM_BUILD_ROOT%{initdir}
mkdir -p $RPM_BUILD_ROOT%{_localstatedir}/run/radvd

install -m 644 redhat/radvd.conf.empty $RPM_BUILD_ROOT%{_sysconfdir}/radvd.conf
install -m 755 redhat/radvd.init $RPM_BUILD_ROOT%{initdir}/radvd
install -m 644 redhat/radvd.sysconfig $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig/radvd

install -d -m 755 $RPM_BUILD_ROOT%{_sysconfdir}/tmpfiles.d
install -p -m 644 redhat/radvd-tmpfs.conf $RPM_BUILD_ROOT%{_sysconfdir}/tmpfiles.d/radvd.conf 

%clean
[ $RPM_BUILD_ROOT != "/" ] && rm -rf $RPM_BUILD_ROOT

%postun
if [ "$1" -ge "1" ]; then
    /sbin/service radvd condrestart >/dev/null 2>&1
fi

%post
/sbin/chkconfig --add radvd

%preun
if [ $1 = 0 ]; then
   /sbin/service radvd stop >/dev/null 2>&1
   /sbin/chkconfig --del radvd
fi

%pre
getent group radvd >/dev/null || groupadd -g %RADVD_UID -r radvd
getent passwd radvd >/dev/null || \
  useradd -r -u %RADVD_UID -g radvd -d / -s /sbin/nologin -c "radvd user" radvd
exit 0

%files
%defattr(-,root,root,-)
%doc COPYRIGHT README CHANGES INTRO.html TODO
%config(noreplace) %{_sysconfdir}/radvd.conf
%config(noreplace) %{_sysconfdir}/sysconfig/radvd
%config(noreplace) %{_sysconfdir}/tmpfiles.d/radvd.conf
%{initdir}/radvd
%dir %attr(-,radvd,radvd) %{_localstatedir}/run/radvd/
%doc radvd.conf.example
%{_mandir}/*/*
%{_sbindir}/radvd
%{_sbindir}/radvdump

%changelog
* Wed Jan 25 2012 Reuben Hawkins <reubenhwk@gmail.com> 1.8.5-1
- 1.8.5
- PID file had wrong PID.  This release fixes the PID.

* Thu Jan 21 2012 Reuben Hawkins <reubenhwk@gmail.com> 1.8.4-1
- 1.8.4
- privsep always on
- minor logging changes

* Fri Oct 14 2011 Reuben Hawkins <reubenhwk@gmail.com> 1.8.3-1
- 1.8.3
- minor fixes

* Wed Oct  4 2011 Reuben Hawkins <reubenhwk@gmail.com> 1.8.2-1
- 1.8.2
- Security patches

* Fri May  6 2011 Reuben Hawkins <reubenhwk@gmail.com> 1.8-1
- 1.8
- IgnoreIfMissing now defaults to on
- Added FlushRoute
- Added DeprecatePrefix
- Added DecrementLifetimes
- Using netlink (on linux) to instantly detect changes in interface states
- Timer code removed, replaces with poll
- build system overhaul
- Some bug fixes

* Sat Jan  1 2011 Pekka Savola <pekkas@netcore.fi> 1.7-1
- 1.7
- Deprecate old, pre-RFC5006 parameters.
- Support RFC6106 by adding DNS Search List support.
- Add '-c' flag to test configuration
- Fix a segmentation fault on reload_config() timer list
  corruption that only occurs with multiple interfaces
- Fix radvd skipping multiple interfaces when
  UnicastOnly is on or AdvSendAdvert is off.
  This got broken in radvd 1.3. Patch from Cedric BAIL
- OSX build
- minor code cleanups

* Fri Mar  5 2010 Pekka Savola <pekkas@netcore.fi> 1.6-1
- 1.6
- Updated the spec file from Fedora (remove userdel at postun),
  use compile flags
- Updated init script from Fedora.

* Thu Sep 10 2009 Pekka Savola <pekkas@netcore.fi> 1.5-1
- 1.5

* Mon Aug  3 2009 Pekka Savola <pekkas@netcore.fi> 1.4-1
- 1.4

* Tue Jul  7 2009 Pekka Savola <pekkas@netcore.fi> 1.3-1
- 1.3; some improvements to the init script

* Tue Oct 28 2008 Pekka Savola <pekkas@netcore.fi> 1.2-1
- 1.2; remove -D_GNU_SOURCE

* Mon Feb  4 2008 Pekka Savola <pekkas@netcore.fi> 1.1-1
- 1.1

* Wed Nov  1 2006 Pekka Savola <pekkas@netcore.fi> 1.0-1
- 1.0; add BuildRequires

* Fri Jan 13 2006 Pekka Savola <pekkas@netcore.fi> 0.9.1-1
- 0.9.1

* Tue Oct 18 2005 Pekka Savola <pekkas@netcore.fi> 0.9-1
- 0.9 (also minor spec file cleanup in %%configure).

* Fri Jul  8 2005 Pekka Savola <pekkas@netcore.fi> 0.8-1
- 0.8.
- Ship the example config file as %%doc (Red Hat's #159005)

* Mon Feb 21 2005 Pekka Savola <pekkas@netcore.fi> 0.7.3-1
- 0.7.3.

* Mon Oct 28 2002 Pekka Savola <pekkas@netcore.fi>
- 0.7.2.

* Tue May  7 2002 Pekka Savola <pekkas@netcore.fi>
- remove '-g %%{RADVD_GID}' when creating the user, which may be problematic
  if the user didn't exist before.

* Wed Jan 14 2002 Pekka Savola <pekkas@netcore.fi>
- 0.7.1.

* Tue Jan  8 2002 Pekka Savola <pekkas@netcore.fi>
- Change 'reload' to signal HUP to radvd instead or restarting.

* Fri Dec 28 2001 Pekka Savola <pekkas@netcore.fi>
- License unfortunately is BSD *with* advertising clause, so to be pedantic,
  change License: to 'BSD-style'.

* Wed Nov 14 2001 Pekka Savola <pekkas@netcore.fi>
- spec file cleanups
- update to 0.7.0.

* Wed Jun 20 2001 Pekka Savola <pekkas@netcore.fi>
- use /sbin/service.
- update to 0.6.2pl4.

* Sat Apr 28 2001 Pekka Savola <pekkas@netcore.fi>
- update to 0.6.2pl3.

* Wed Apr 11 2001 Pekka Savola <pekkas@netcore.fi>
- update to 0.6.2pl2.

* Wed Apr  4 2001 Pekka Savola <pekkas@netcore.fi>
- update to 0.62pl1.  Bye bye patches!
- Require: initscripts (should really be with a version providing IPv6)
- clean up the init script, make condrestart work properly
- Use a static /etc/rc.d/init.d; init.d/radvd required it anyway.

* Sun Apr  1 2001 Pekka Savola <pekkas@netcore.fi>
- add patch to chroot (doesn't work well yet, as /proc is used directly)
- clean up droproot patch, drop the rights earlier; require user-writable
pidfile directory
- set up the pidfile directory at compile time.

* Sat Mar 31 2001 Pekka Savola <pekkas@netcore.fi>
- add select/kill signals patch from Nathan Lutchansky <lutchann@litech.org>.
- add address syntax checked fix from Marko Myllynen <myllynen@lut.fi>.
- add patch to check the pid file before fork.
- add support for OPTIONS sourced from /etc/sysconfig/radvd, provide a nice
default one.
- add/delete radvd user, change the pidfile to /var/run/radvd/radvd.pid.
- fix initscript NETWORKING_IPV6 check.

* Sun Mar 18 2001 Pekka Savola <pekkas@netcore.fi>
- add droproot patch, change to nobody by default (should use radvd:radvd or
the like, really).

* Mon Mar  5 2001 Tim Powers <timp@redhat.com>
- applied patch supplied by Pekka Savola in #30508
- made changes to initscript as per Pekka's suggestions

* Thu Feb 15 2001 Tim Powers <timp@redhat.com>
- needed -D_GNU_SOURCE to build properly

* Tue Feb  6 2001 Tim Powers <timp@redhat.com>
- use %%configure and %%makeinstall, just glob the manpages, cleans
  things up
- fixed initscript so that it can be internationalized in the future

* Fri Feb 2 2001 Pekka Savola <pekkas@netcore.fi>
- Create a single package(source) for glibc21 and glibc22 (automatic
Requires can handle this just fine).
- use %%{_mandir} and friends
- add more flesh to %doc
- streamline %config file %attrs
- streamline init.d file a bit:
   * add a default chkconfig: (default to disable for security etc. reasons; 
     also, the default config isn't generic enough..)
   * add reload/condrestart
   * minor tweaks
   * missing: localization support (initscripts-5.60)
- use %%initdir macro

* Thu Feb 1 2001 Lars Fenneberg <lf@elemental.net>
- updated to new release 0.6.2

* Thu Feb 1 2001 Marko Myllynen <myllynen@lut.fi>
- initial version, radvd version 0.6.1
