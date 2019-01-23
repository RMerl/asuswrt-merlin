Summary: ncurses-examples - example/test programs from ncurses
%define AppProgram ncurses-examples
%define AppVersion MAJOR.MINOR
%define AppRelease YYYYMMDD
# $Id: ncurses-examples.spec,v 1.5 2015/04/25 10:55:22 tom Exp $
Name: %{AppProgram}
Version: %{AppVersion}
Release: %{AppRelease}
License: MIT
Group: Applications/Development
URL: ftp://invisible-island.net/%{AppProgram}
Source0: %{AppProgram}-%{AppRelease}.tgz
Packager: Thomas Dickey <dickey@invisible-island.net>

%description
These are the example/test programs from the ncurses MAJOR.MINOR distribution,
for patch-date YYYYMMDD.

This package installs in "bin/ncurses-examples" to avoid conflict with other
packages.
%prep

%setup -q -n %{AppProgram}-%{AppRelease}

%build

INSTALL_PROGRAM='${INSTALL}' \
	./configure \
		--target %{_target_platform} \
		--prefix=%{_prefix} \
		--bindir=%{_bindir}/%{AppProgram} \
		--with-screen=ncursesw6 \
		--disable-rpath-hack

make

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

make install               DESTDIR=$RPM_BUILD_ROOT

strip $RPM_BUILD_ROOT%{_bindir}/%{AppProgram}/*

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%{_bindir}/%{AppProgram}/*

%changelog
# each patch should add its ChangeLog entries here

* Thu Mar 25 2010 Thomas Dickey
- initial version
