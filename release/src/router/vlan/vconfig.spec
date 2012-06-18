Summary: Linux 802.1q VLAN configuration utility
Name: vconfig
Version: 1.6
Release: 4
License: distributable
Group: Applications/System
Source: http://scry.wanfear.com/~greear/vlan/vlan.%{version}.tar.gz
Source1: ifup-vlan
Source2: ifdown-vlan
Source3: vlan.sysconfig
Source4: README.ifup
Source5: ifcfg-vlan2-example
URL: http://scry.wanfear.com/~greear/vlan.html
BuildRoot: %{_tmppath}/%{name}-%{version}-root
Packager: Dale Bewley <dale@bewley.net>
BuildRequires: kernel-source >= 2.4.14
Requires: kernel >= 2.4.14

%description 
802.1q VLAN support is now in the linux kernel as of 2.4.14. 
This package provides the vlan configuration utility, vconfig,
and ifup and ifdown scripts for configuring interfaces.

%prep
%setup -q -n vlan
cp %SOURCE4 .
cp %SOURCE5 .

%build
make

%clean 
rm -rf $RPM_BUILD_ROOT

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{_sbindir}
mkdir -p $RPM_BUILD_ROOT/etc/sysconfig/network-scripts
mkdir -p $RPM_BUILD_ROOT/%{_mandir}/man8
install -o 0 -g 0 -m 755 vconfig $RPM_BUILD_ROOT/%{_sbindir}/vconfig
install -o 0 -g 0 -m 755 vconfig.8 $RPM_BUILD_ROOT/%{_mandir}/man8
install -o 0 -g 0 -m 755 %SOURCE1 $RPM_BUILD_ROOT/etc/sysconfig/network-scripts/ifup-vlan
install -o 0 -g 0 -m 755 %SOURCE2 $RPM_BUILD_ROOT/etc/sysconfig/network-scripts/ifdown-vlan
install -o 0 -g 0 -m 644 %SOURCE3 $RPM_BUILD_ROOT/etc/sysconfig/vlan

%files 
%defattr(-,root,root)
%doc CHANGELOG contrib README README.ifup vlan.html vlan_test.pl ifcfg-vlan2-example
%{_sbindir}/vconfig
%{_mandir}/man8/vconfig.8.gz
/etc/sysconfig/vlan
/etc/sysconfig/network-scripts/ifup-vlan
/etc/sysconfig/network-scripts/ifdown-vlan

%changelog
* Fri Apr 05 2002 Dale Bewley <dale@bewley.net>
- update to 1.6
- add ifup scripts

* Tue Dec 11 2001 Dale Bewley <dale@bewley.net>
- initial specfile
