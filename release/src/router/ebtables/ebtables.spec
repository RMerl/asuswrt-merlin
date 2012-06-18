# spec file originally from Dag Wieers, altered by Bart De Schuymer

%define _sbindir /usr/local/sbin
%define _mysysconfdir %{_sysconfdir}/sysconfig

Summary: Ethernet Bridge frame table administration tool
Name: ebtables
Version: 2.0.9
Release: 2
License: GPL
Group: System Environment/Base
URL: http://ebtables.sourceforge.net/

Packager: Bart De Schuymer <bdschuym@pandora.be>

Source: http://dl.sf.net/ebtables/ebtables-v%{version}-%{release}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root

%description
Ethernet bridge tables is a firewalling tool to transparantly filter network
traffic passing a bridge. The filtering possibilities are limited to link
layer filtering and some basic filtering on higher network layers.

The ebtables tool can be used together with the other Linux filtering tools,
like iptables. There are no incompatibility issues.

%prep
%setup -n ebtables-v%{version}-%{release}

%build
%{__make} %{?_smp_mflags} \
	CFLAGS="%{optflags}"

%install
%{__rm} -rf %{buildroot}
%{__install} -D -m0755 ebtables %{buildroot}%{_sbindir}/ebtables
%{__install} -D -m0755 ebtables-restore %{buildroot}%{_sbindir}/ebtables-restore
%{__install} -D -m0644 ethertypes %{buildroot}%{_sysconfdir}/ethertypes
%{__install} -D -m0644 ebtables.8 %{buildroot}%{_mandir}/man8/ebtables.8
%{__mkdir} -p %{buildroot}%{_libdir}/ebtables/
%{__mkdir} -p %{buildroot}%{_sbindir}
%{__mkdir} -p %{buildroot}%{_initrddir}
%{__mkdir} -p %{buildroot}%{_mysysconfdir}
%{__install} -m0755 extensions/*.so %{buildroot}%{_libdir}/ebtables/
%{__install} -m0755 *.so %{buildroot}%{_libdir}/ebtables/
export __iets=`printf %{_sbindir} | sed 's/\\//\\\\\\//g'`
export __iets2=`printf %{_mysysconfdir} | sed 's/\\//\\\\\\//g'`
sed -i "s/__EXEC_PATH__/$__iets/g" ebtables-save
%{__install} -m 0755 -o root -g root ebtables-save %{buildroot}%{_sbindir}/ebtables-save
sed -i "s/__EXEC_PATH__/$__iets/g" ebtables.sysv; sed -i "s/__SYSCONFIG__/$__iets2/g" ebtables.sysv
%{__install} -m 0755 -o root -g root ebtables.sysv %{buildroot}%{_initrddir}/ebtables
sed -i "s/__SYSCONFIG__/$__iets2/g" ebtables-config
%{__install} -m 0600 -o root -g root ebtables-config %{buildroot}%{_mysysconfdir}/ebtables-config
unset __iets
unset __iets2

%clean
%{__rm} -rf %{buildroot}

%post
/sbin/chkconfig --add ebtables

%preun
if [ $1 -eq 0 ]; then
	/sbin/service ebtables stop &>/dev/null || :
	/sbin/chkconfig --del ebtables
fi

%files
%defattr(-, root, root, 0755)
%doc ChangeLog COPYING INSTALL THANKS
%doc %{_mandir}/man8/ebtables.8*
%config %{_sysconfdir}/ethertypes
%config %{_mysysconfdir}/ebtables-config
%config %{_initrddir}/ebtables
%{_sbindir}/ebtables
%{_sbindir}/ebtables-save
%{_sbindir}/ebtables-restore
%{_libdir}/ebtables/

%changelog
* Mon Nov 07 2005 Bart De Schuymer <bdschuym@pandora.be> - 2.0.8-rc1
- Initial package.
