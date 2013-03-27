Summary: Tracks and displays system calls associated with a running process
Name: strace
Version: 4.6
Release: 1%{?dist}
License: BSD
Group: Development/Debuggers
URL: http://sourceforge.net/projects/strace/
Source: http://downloads.sourceforge.net/strace/%{name}-%{version}.tar.xz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires: libacl-devel, libaio-devel, time

%define strace64_arches ppc64 sparc64

%description
The strace program intercepts and records the system calls called and
received by a running process.  Strace can print a record of each
system call, its arguments and its return value.  Strace is useful for
diagnosing problems and debugging, as well as for instructional
purposes.

Install strace if you need a tool to track the system calls made and
received by a process.

%ifarch %{strace64_arches}
%package -n strace64
Summary: Tracks and displays system calls associated with a running process.
Group: Development/Debuggers

%description -n strace64
The strace program intercepts and records the system calls called and
received by a running process.  Strace can print a record of each
system call, its arguments and its return value.  Strace is useful for
diagnosing problems and debugging, as well as for instructional
purposes.

Install strace if you need a tool to track the system calls made and
received by a process.

This package provides the `strace64' program to trace 64-bit processes.
The `strace' program in the `strace' package is for 32-bit processes.
%endif

%prep
%setup -q

%build
%configure
make %{?_smp_mflags}

%install
rm -rf %{buildroot}
make DESTDIR=%{buildroot} install

# remove unpackaged files from the buildroot
rm -f %{buildroot}%{_bindir}/strace-graph

%define copy64 ln
%if 0%{?rhel}
%if 0%{?rhel} < 6
%endif
%define copy64 cp -p
%endif

%ifarch %{strace64_arches}
%{copy64} %{buildroot}%{_bindir}/strace %{buildroot}%{_bindir}/strace64
%endif

%check
make check

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%doc CREDITS ChangeLog ChangeLog-CVS COPYRIGHT NEWS PORTING README
%{_bindir}/strace
%{_mandir}/man1/*

%ifarch %{strace64_arches}
%files -n strace64
%defattr(-,root,root)
%{_bindir}/strace64
%endif

%changelog
* Mon Mar 14 2011 Dmitry V. Levin <ldv@altlinux.org> - 4.6-1
- New upstream release.
  + fixed a corner case in waitpid handling (#663547).

* Wed Feb 09 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 4.5.20-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Tue Apr 13 2010 Roland McGrath <roland@redhat.com> - 4.5.20-1
- New upstream release, work mostly by Andreas Schwab and Dmitry V. Levin.
  + fixed potential stack buffer overflow in select decoder (#556678);
  + fixed FTBFS (#539044).

* Wed Oct 21 2009 Roland McGrath <roland@redhat.com> - 4.5.19-1
- New upstream release, work mostly by Dmitry V. Levin <ldv@altlinux.org>
  + exit/kill strace with traced process exitcode/signal (#105371);
  + fixed build on ARM EABI (#507576);
  + fixed display of 32-bit argv array on 64-bit architectures (#519480);
  + fixed display of 32-bit fcntl(F_SETLK) on 64-bit architectures (#471169);
  + fixed several bugs in strings decoder, including potential heap
    memory corruption (#470529, #478324, #511035).

* Thu Aug 28 2008 Roland McGrath <roland@redhat.com> - 4.5.18-1
- build fix for newer kernel headers (#457291)
- fix CLONE_VFORK handling (#455078)
- Support new Linux/PPC system call subpage_prot and PROT_SAO flag.
- In sigaction system call, display sa_flags value along with SIG_DFL/SIG_IGN.

* Mon Jul 21 2008 Roland McGrath <roland@redhat.com> - 4.5.17-1
- handle O_CLOEXEC, MSG_CMSG_CLOEXEC (#365781)
- fix biarch stat64 decoding (#222275)
- fix spurious "..." in printing of environment strings (#358241)
- improve prctl decoding (#364401)
- fix hang wait on exited child with exited child (#354261)
- fix biarch fork/vfork (-f) tracing (#447475)
- fix biarch printing of negative argument kill (#430585)
- fix biarch decoding of error return values (#447587)
- fix -f tracing of CLONE_VFORK (#455078)
- fix ia64 register clobberation in -f tracing (#453438)
- print SO_NODEFER, SA_RESETHAND instead of SA_NOMASK, SA_ONESHOT (#455821)
- fix futex argument decoding (#448628, #448629)

* Fri Aug  3 2007 Roland McGrath <roland@redhat.com> - 4.5.16-1
- fix multithread issues (#240962, #240961, #247907)
- fix spurious SIGSTOP on early interrupt (#240986)
- fix utime for biarch (#247185)
- fix -u error message (#247170)
- better futex syscall printing (##241467)
- fix argv/envp printing with small -s settings, and for biarch
- new syscalls: getcpu, eventfd, timerfd, signalfd, epoll_pwait,
  move_pages, utimensat

* Tue Jan 16 2007 Roland McGrath <roland@redhat.com> - 4.5.15-1
- biarch fixes (#179740, #192193, #171626, #173050, #218433, #218043)
- fix -ff -o behavior (#204950, #218435, #193808, #219423)
- better quotactl printing (#118696)
- *at, inotify*, pselect6, ppoll and unshare syscalls (#178633, #191275)
- glibc-2.5 build fixes (#209856)
- memory corruption fixes (#200621
- fix race in child setup under -f (#180293)
- show ipc key values in hex (#198179, #192182)
- disallow -c with -ff (#187847)
- Resolves: RHBZ #179740, RHBZ #192193, RHBZ #204950, RHBZ #218435
- Resolves: RHBZ #193808, RHBZ #219423, RHBZ #171626, RHBZ #173050
- Resolves: RHBZ #218433, RHBZ #218043, RHBZ #118696, RHBZ #178633
- Resolves: RHBZ #191275, RHBZ #209856, RHBZ #200621, RHBZ #180293
- Resolves: RHBZ #198179, RHBZ #198182, RHBZ #187847

* Mon Nov 20 2006 Jakub Jelinek <jakub@redhat.com> - 4.5.14-4
- Fix ia64 syscall decoding (#206768)
- Fix build with glibc-2.4.90-33 and up on all arches but ia64
- Fix build against 2.6.18+ headers

* Tue Aug 22 2006 Roland McGrath <roland@redhat.com> - 4.5.14-3
- Fix bogus decoding of syscalls >= 300 (#201462, #202620).

* Fri Jul 14 2006 Jesse Keating <jkeating@redhat.com> - 4.5.14-2
- rebuild

* Fri Feb 10 2006 Jesse Keating <jkeating@redhat.com> - 4.5.14-1.2
- bump again for long double bug on ppc{,64}

* Tue Feb 07 2006 Jesse Keating <jkeating@redhat.com> - 4.5.14-1.1
- rebuilt for new gcc4.1 snapshot and glibc changes

* Mon Jan 16 2006 Roland McGrath <roland@redhat.com> - 4.5.14-1
- Fix biarch decoding of socket syscalls (#174354).
- Fix biarch -e support (#173986).
- Accept numeric syscalls in -e (#174798).
- Fix ipc syscall decoding (#164755).
- Improve msgrcv printing (#164757).
- Man page updates (#165375).
- Improve mount syscall printing (#165377).
- Correct printing of restarting syscalls (#165469).

* Wed Aug  3 2005 Roland McGrath <roland@redhat.com> - 4.5.13-1
- Fix setsockopt decoding on 64-bit (#162449).
- Fix typos in socket option name strings (#161578).
- Display more IPV6 socket options by name (#162450).
- Don't display inappropriate syscalls for -e trace=file (#159340).
- New selector type -e trace=desc for file-descriptor using calls (#159400).
- Fix 32-bit old_mmap syscall decoding on x86-64 (#162467, #164215).
- Fix errors detaching from multithreaded process on interrupt (#161919).
- Note 4.5.12 fix for crash handling bad signal numbers (#162739).

* Wed Jun  8 2005 Roland McGrath <roland@redhat.com> - 4.5.12-1
- Fix known syscall recognition for IA32 processes on x86-64 (#158934).
- Fix bad output for ptrace on x86-64 (#159787).
- Fix potential buffer overruns (#151570, #159196).
- Make some diagnostics more consistent (#159308).
- Update PowerPC system calls.
- Better printing for Linux aio system calls.
- Don't truncate statfs64 fields to 32 bits in output (#158243).
- Cosmetic code cleanups (#159688).

* Tue Mar 22 2005 Roland McGrath <roland@redhat.com> - 4.5.11-1
- Build tweaks.
- Note 4.5.10 select fix (#151570).

* Mon Mar 14 2005 Roland McGrath <roland@redhat.com> - 4.5.10-1
- Fix select handling on nonstandard fd_set sizes.
- Don't print errors for null file name pointers.
- Fix initial execve output with -i (#143365).

* Fri Feb  4 2005 Roland McGrath <roland@redhat.com> - 4.5.9-2
- update ia64 syscall list (#146245)
- fix x86_64 syscall argument extraction for 32-bit processes (#146093)
- fix -e signal=NAME parsing (#143362)
- fix x86_64 exit_group syscall handling
- improve socket ioctl printing (#138223)
- code cleanups (#143369, #143370)
- improve mount flags printing (#141932)
- support symbolic printing of x86_64 arch_prctl parameters (#142667)
- fix potential crash in getxattr printing

* Tue Oct 19 2004 Roland McGrath <roland@redhat.com> - 4.5.8-1
- fix multithreaded exit handling (#132150, #135254)
- fix ioctl name matching (#129808)
- print RTC_* ioctl structure contents (#58606)
- grok epoll_* syscalls (#134463)
- grok new RLIMIT_* values (#133594)
- print struct cmsghdr contents for sendmsg (#131689)
- fix clock_* and timer_* argument output (#131420)

* Tue Aug 31 2004 Roland McGrath <roland@redhat.com> - 4.5.7-2
- new upstream version, misc fixes and updates (#128091, #129166, #128391, #129378, #130965, #131177)

* Mon Jul 12 2004 Roland McGrath <roland@redhat.com> 4.5.6-1
- new upstream version, updates ioctl lists (#127398), fixes quotactl (#127393), more ioctl decoding (#126917)

* Sun Jun 27 2004 Roland McGrath <roland@redhat.com> 4.5.5-1
- new upstream version, fixes x86-64 biarch support (#126547)

* Tue Jun 15 2004 Elliot Lee <sopwith@redhat.com> 4.5.4-2
- rebuilt

* Thu Jun  3 2004 Roland McGrath <roland@redhat.com> 4.5.4-0.FC1
- rebuilt for FC1 update

* Thu Jun  3 2004 Roland McGrath <roland@redhat.com> 4.5.4-1
- new upstream version, more ioctls (#122257), minor fixes

* Fri Apr 16 2004 Roland McGrath <roland@redhat.com> 4.5.3-1
- new upstream version, mq_* calls (#120701), -p vs NPTL (#120462), more fixes (#118694, #120541, #118685)

* Tue Mar 02 2004 Elliot Lee <sopwith@redhat.com> 4.5.2-1.1
- rebuilt

* Mon Mar  1 2004 Roland McGrath <roland@redhat.com> 4.5.2-1
- new upstream version, sched_* calls (#116990), show core flag (#112117)

* Fri Feb 13 2004 Elliot Lee <sopwith@redhat.com>
- rebuilt

* Thu Nov 13 2003 Roland McGrath <roland@redhat.com> 4.5.1-1
- new upstream version, more fixes (#108012, #105366, #105359, #105358)

* Tue Sep 30 2003 Roland McGrath <roland@redhat.com> 4.5-3
- revert bogus s390 fix

* Thu Sep 25 2003 Roland McGrath <roland@redhat.com> 4.5-1.2.1AS
- rebuilt for 2.1AS erratum

* Wed Sep 24 2003 Roland McGrath <roland@redhat.com> 4.5-2
- rebuilt

* Wed Sep 24 2003 Roland McGrath <roland@redhat.com> 4.5-1
- new upstream version, more fixes (#101499, #104365)

* Thu Jul 17 2003 Roland McGrath <roland@redhat.com> 4.4.99-2
- rebuilt

* Thu Jul 17 2003 Roland McGrath <roland@redhat.com> 4.4.99-1
- new upstream version, groks more new system calls, PF_INET6 sockets

* Mon Jun 10 2003 Roland McGrath <roland@redhat.com> 4.4.98-1
- new upstream version, more fixes (#90754, #91085)

* Wed Jun 04 2003 Elliot Lee <sopwith@redhat.com>
- rebuilt

* Sun Mar 30 2003 Roland McGrath <roland@redhat.com> 4.4.96-1
- new upstream version, handles yet more 2.5 syscalls, x86_64 & ia64 fixes

* Mon Feb 24 2003 Elliot Lee <sopwith@redhat.com> 4.4.95-2
- rebuilt

* Mon Feb 24 2003 Roland McGrath <roland@redhat.com> 4.4.95-1
- new upstream version, fixed getresuid/getresgid (#84959)

* Wed Feb 19 2003 Roland McGrath <roland@redhat.com> 4.4.94-1
- new upstream version, new option -E to set environment variables (#82392)

* Wed Jan 22 2003 Tim Powers <timp@redhat.com> 4.4.93-2
- rebuilt

* Tue Jan 21 2003 Roland McGrath <roland@redhat.com> 4.4.93-1
- new upstream version, fixes ppc and s390 bugs, adds missing ptrace requests

* Fri Jan 10 2003 Roland McGrath <roland@redhat.com> 4.4.91-1
- new upstream version, fixes -f on x86-64

* Fri Jan 10 2003 Roland McGrath <roland@redhat.com> 4.4.90-1
- new upstream version, fixes all known bugs modulo ia64 and s390 issues

* Fri Jan 03 2003 Florian La Roche <Florian.LaRoche@redhat.de> 4.4-11
- add further s390 patch from IBM

* Wed Nov 27 2002 Tim Powers <timp@redhat.com> 4.4-10
- remove unpackaged files from the buildroot

* Mon Oct 07 2002 Phil Knirsch <pknirsch@redhat.com> 4.4-9.1
- Added latest s390(x) patch.

* Fri Sep 06 2002 Karsten Hopp <karsten@redhat.de> 4.4-9
- preliminary x86_64 support with an ugly patch to help
  debugging. Needs cleanup!

* Mon Sep  2 2002 Jakub Jelinek <jakub@redhat.com> 4.4-8
- newer version of the clone fixing patch (Roland McGrath)
- aio syscalls for i386/ia64/ppc (Ben LaHaise)

* Wed Aug 28 2002 Jakub Jelinek <jakub@redhat.com> 4.4-7
- fix strace -f (Roland McGrath, #68994)
- handle ?et_thread_area, SA_RESTORER (Ulrich Drepper)

* Fri Jun 21 2002 Jakub Jelinek <jakub@redhat.com> 4.4-6
- handle futexes, *xattr, sendfile64, etc. (Ulrich Drepper)
- handle modify_ldt (#66894)

* Thu May 23 2002 Tim Powers <timp@redhat.com>
- automated rebuild

* Tue Apr 16 2002 Jakub Jelinek <jakub@redhat.com> 4.4-4
- fix for the last patch by Jeff Law (#62591)

* Mon Mar  4 2002 Preston Brown <pbrown@redhat.com> 4.4-3
- integrate patch from Jeff Law to eliminate hang tracing threads

* Sat Feb 23 2002 Florian La Roche <Florian.LaRoche@redhat.de>
- minor update from debian tar-ball

* Wed Jan 02 2002 Florian La Roche <Florian.LaRoche@redhat.de>
- update to 4.4

* Sun Jul 22 2001 Florian La Roche <Florian.LaRoche@redhat.de>
- disable s390 patches, they are already included

* Wed Jul 18 2001 Preston Brown <pbrown@redhat.com> 4.3-1
- new upstream version.  Seems to have integrated most new syscalls
- tracing threaded programs is now functional.

* Mon Jun 11 2001 Than Ngo <than@redhat.com>
- port s390 patches from IBM

* Wed May 16 2001 Nalin Dahyabhai <nalin@redhat.com>
- modify new syscall patch to allocate enough heap space in setgroups32()

* Wed Feb 14 2001 Jakub Jelinek <jakub@redhat.com>
- #include <time.h> in addition to <sys/time.h>

* Fri Jan 26 2001 Karsten Hopp <karsten@redhat.com>
- clean up conflicting patches. This happened only
  when building on S390

* Fri Jan 19 2001 Bill Nottingham <notting@redhat.com>
- update to CVS, reintegrate ia64 support

* Sat Dec  8 2000 Bernhard Rosenkraenzer <bero@redhat.com>
- Get S/390 support into the normal package

* Sat Nov 18 2000 Florian La Roche <Florian.LaRoche@redhat.de>
- added S/390 patch from IBM, adapting it to not conflict with
  IA64 patch

* Sat Aug 19 2000 Jakub Jelinek <jakub@redhat.com>
- doh, actually apply the 2.4 syscalls patch
- make it compile with 2.4.0-test7-pre4+ headers, add
  getdents64 and fcntl64

* Thu Aug  3 2000 Jakub Jelinek <jakub@redhat.com>
- add a bunch of new 2.4 syscalls (#14036)

* Wed Jul 12 2000 Prospector <bugzilla@redhat.com>
- automatic rebuild
- excludearch ia64

* Fri Jun  2 2000 Matt Wilson <msw@redhat.com>
- use buildinstall for FHS

* Wed May 24 2000 Jakub Jelinek <jakub@redhat.com>
- make things compile on sparc
- fix sigreturn on sparc

* Fri Mar 31 2000 Bill Nottingham <notting@redhat.com>
- fix stat64 misdef (#10485)

* Tue Mar 21 2000 Michael K. Johnson <johnsonm@redhat.com>
- added ia64 patch

* Thu Feb 03 2000 Cristian Gafton <gafton@redhat.com>
- man pages are compressed
- version 4.2 (why are we keeping all these patches around?)

* Sat Nov 27 1999 Jeff Johnson <jbj@redhat.com>
- update to 4.1 (with sparc socketcall patch).

* Fri Nov 12 1999 Jakub Jelinek <jakub@redhat.com>
- fix socketcall on sparc.

* Thu Sep 02 1999 Cristian Gafton <gafton@redhat.com>
- fix KERN_SECURELVL compile problem

* Tue Aug 31 1999 Cristian Gafton <gafton@redhat.com>
- added alpha patch from HJLu to fix the osf_sigprocmask interpretation

* Sat Jun 12 1999 Jeff Johnson <jbj@redhat.com>
- update to 3.99.1.

* Wed Jun  2 1999 Jeff Johnson <jbj@redhat.com>
- add (the other :-) jj's sparc patch.

* Wed May 26 1999 Jeff Johnson <jbj@redhat.com>
- upgrade to 3.99 in order to
-    add new 2.2.x open flags (#2955).
-    add new 2.2.x syscalls (#2866).
- strace 3.1 patches carried along for now.

* Sun May 16 1999 Jeff Johnson <jbj@redhat.com>
- don't rely on (broken!) rpm %%patch (#2735)

* Tue Apr 06 1999 Preston Brown <pbrown@redhat.com>
- strip binary

* Sun Mar 21 1999 Cristian Gafton <gafton@redhat.com>
- auto rebuild in the new build environment (release 16)

* Tue Feb  9 1999 Jeff Johnson <jbj@redhat.com>
- vfork est arrive!

* Tue Feb  9 1999 Christopher Blizzard <blizzard@redhat.com>
- Add patch to follow clone() syscalls, too.

* Sun Jan 17 1999 Jeff Johnson <jbj@redhat.com>
- patch to build alpha/sparc with glibc 2.1.

* Thu Dec 03 1998 Cristian Gafton <gafton@redhat.com>
- patch to build on ARM

* Wed Sep 30 1998 Jeff Johnson <jbj@redhat.com>
- fix typo (printf, not tprintf).

* Sat Sep 19 1998 Jeff Johnson <jbj@redhat.com>
- fix compile problem on sparc.

* Tue Aug 18 1998 Cristian Gafton <gafton@redhat.com>
- buildroot

* Mon Jul 20 1998 Cristian Gafton <gafton@redhat.com>
- added the umoven patch from James Youngman <jay@gnu.org>
- fixed build problems on newer glibc releases

* Mon Jun 08 1998 Prospector System <bugs@redhat.com>
- translations modified for de, fr, tr
