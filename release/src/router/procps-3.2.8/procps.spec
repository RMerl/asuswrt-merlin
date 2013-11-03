URL: http://procps.sf.net/
Summary: System and process monitoring utilities
Name: procps
%define major_version 3
%define minor_version 2
%define revision 8
%define version %{major_version}.%{minor_version}.%{revision}
Version: %{version}
Release: 1
License: LGPL, GPL, BSD-like
Group: Applications/System
Source: http://procps.sf.net/procps-%{version}.tar.gz
BuildRoot: %{_tmppath}/procps-root
Packager: <procps-feedback@lists.sf.net>

%description
The procps package contains a set of system utilities which provide
system information.  Procps includes ps, free, sysctl, skill, snice,
tload, top, uptime, vmstat, w, and watch. You need some of these.

%prep
%setup

%build
make SKIP="/bin/kill /usr/share/man/man1/kill.1" CFLAGS="$RPM_OPT_FLAGS"

%install
rm -rf $RPM_BUILD_ROOT
make SKIP="/bin/kill /usr/share/man/man1/kill.1" DESTDIR=$RPM_BUILD_ROOT ldconfig=echo install="install -D" lib="$RPM_BUILD_ROOT/%{_lib}/" install

%clean
rm -rf $RPM_BUILD_ROOT

%post
# add libproc to the cache
/sbin/ldconfig

%files
%defattr(0644,root,root,755)
%doc NEWS BUGS TODO COPYING COPYING.LIB README.top README AUTHORS sysctl.conf
%attr(555,root,root) /lib*/libproc*.so*
%attr(555,root,root) /bin/*
%attr(555,root,root) /sbin/*
%attr(555,root,root) /usr/bin/*

%attr(0644,root,root) /usr/share/man/man1/*
%attr(0644,root,root) /usr/share/man/man5/*
%attr(0644,root,root) /usr/share/man/man8/*

%changelog
* Fri Apr 14 09:23:45 PDT 2006  Jesse Brandeburg <jesse.brandeburg@in...>
- fix missing trailing slash in %install to fix builds on x86_64
