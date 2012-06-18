Summary: p910nd is a small daemon that copies any data received to the corresponding printer port.
Name: p910nd
Version: 0.93
Release: 1
URL: http://etherboot.sourceforge.net/p910nd
Vendor: Ken Yap
License: GPL v2
Source0: http://etherboot.sourceforge.net/p910nd/%{name}-%{version}.tar.gz
BuildArchitectures: i386 x86_64
BuildRoot: %{_tmppath}/%{name}-%{version}-build
Group: Networking

%description
p910nd implements the port 9100 network printer protocol which simply
copies any incoming data on the port to the printer (and in the reverse
direction, if bidirectional mode is selected). Both parallel and USB
printers are supported. This protocol was used in HP's printers and is
called JetDirect (probably TM). p910nd is particularly useful for
diskless hosts and embedded devices because it does not require any disk
space for spooling as this is done at the sending host.

%pre

%prep
rm -fr %{buildroot}
%setup -n p910nd-%{version}

%build
make

%install
%makeinstall

%post
insserv /etc/init.d/p910nd

%preun
insserv -r /etc/init.d/p910nd

%postun

%clean
rm -fr %{buildroot}

%files
%defattr(755, root, root)
/etc/sysconfig/p910nd
/etc/init.d/p910nd
/usr/sbin/p910nd
%{_mandir}/man8/p910nd.8.gz

%changelog
* Fri Jan 04 2008 Ken Yap <greenpossum@users.sourceforge.net>
- 0.92: First spec file
* Mon Feb 09 2009 Ken Yap <greenpossum@users.sourceforge.net>
- 0.93: Fix up open with mode for call with O_CREAT
