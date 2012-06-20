Summary: The Wake On Lan client.
Name: wol
Version: 0.7.1
Release: 1
Copyright: GPL
Group: Application/Networking
Source: %{name}-%{version}.tar.gz
Url: http://ahh.sourceforge.net/wol/
Packager: Thomas Hager <duke@bofh.at>
BuildRoot: /var/tmp/%{name}-root

%description
This is wol, the Wake On Lan client. It wakes up magic packet compliant
machines such as boxes with wake-on-lan ethernet-cards. Some workstations
provides SecureON which extends wake-on-lan with a password. This feature
is also provided by wol.

%prep
%setup

%build
%configure
make

%install
if [ -d $RPM_BUILD_ROOT -a $RPM_BUILD_ROOT != "/" ]; then
        rm -rf $RPM_BUILD_ROOT
fi
make DESTDIR=$RPM_BUILD_ROOT install

%clean
if [ -d $RPM_BUILD_ROOT -a $RPM_BUILD_ROOT != "/" ]; then
        rm -rf $RPM_BUILD_ROOT
fi

if [ -d $RPM_BUILD_DIR/%{name}-%{version} -a $RPM_BUILD_DIR/%{name}-%{version} != "/" ]; then
        rm -rf $RPM_BUILD_DIR/%{name}-%{version}
fi

%files
%doc ABOUT-NLS AUTHORS ChangeLog COPYING NEWS README TODO
%{_infodir}/wol.*
%{_mandir}/*/*
%{_bindir}/*
%{_datadir}/locale/*/*/*

%changelog
* Sun Apr 18 2004 Thomas Krennwallner <krennwallner@aon.at>
- new version 0.7.1

* Mon Aug 11 2003 Thomas Hager <duke@bofh.at>
- fixed man path in %files section

* Sat Aug 09 2003 Thomas Krennwallner <krennwallner@aon.at>
- new version 0.7.0
- manpage added

* Wed Jul 09 2003 Thomas Hager <duke@bofh.at>
- added wol-pathname.patch

* Fri May 09 2003 Thomas Hager <duke@bofh.at>
- removed $SOURCE/doc directory from %doc macro

* Thu May 08 2003 Thomas Hager <duke@bofh.at>
- initial release
