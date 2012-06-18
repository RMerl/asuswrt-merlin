Name: l7-protocols
Summary: Protocol definitions files for l7-filter
Version: FILL_THIS_IN_WITH_THE_VERSION_NUMBER
Release: 1
License: GPL
Group: Applications/Internet
URL: http://l7-filter.sourceforge.net/
Source0: http://prdownloads.sf.net/l7-filter/%name-%version.tar.gz
BuildRoot: %{_tmppath}/%{name}-buildroot

%description
Protocol definitions files for use with the Linux Layer 7 Packet Classifier.
These files are regular expressions that define Internet protocols such as
HTTP, MSN Messenger, FTP, Cisco VPN, Fasttrack, DNS, Gnutella, Quake, etc.

%prep
%setup -q

%build

%install
rm -rf $RPM_BUILD_ROOT 
make PREFIX=$RPM_BUILD_ROOT install

%clean

%files
%defattr(-, root, root)
/etc/l7-protocols/

%changelog
* Thu Dec 08 2004 FIRSTNAME LASTNAME <EMAILADDRESS> VERSION-1
- Upgrade to VERSION
* Wed Jul 07 2004 Matthew Strait <quadong@users.sf.net> 2004_07_07-1
- Initial RPM

