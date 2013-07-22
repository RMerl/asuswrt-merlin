Summary: Tools for maintaining Memory Technology Devices
Name: mtd-utils
Version: 1.0
Release: 1
License: GPL
Group: Applications/System
URL: http://www.linux-mtd.infradead.org/
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

%description
This package contains tools for erasing and formatting flash devices,
including JFFS2, M-Systems DiskOnChip devices, etc.

%prep
%setup -q

%build
make -C util

%install
rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT -C util install

%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root,-)
/usr/sbin
/usr/man/man1/mkfs.jffs2.1.gz
/usr/include/mtd
%doc


%changelog
* Wed May  5 2004  <dwmw2@infradead.org> - 1.0
- Initial build.

