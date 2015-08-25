Summary: A multi-threaded implementation of Apple's DAAP server
Name: mt-daapd
Version: 0.2.1.1
Release: 1
License: GPL
Group: Development/Networking
URL: http://sourceforge.net/project/showfiles.php?group_id=98211
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot
Requires: libid3tag gdbm
BuildRequires: libid3tag-devel gdbm-devel

%description
A multi-threaded implementation of Apple's DAAP server, mt-daapd
allows a Linux machine to advertise MP3 files to to used by 
Windows or Mac iTunes clients.  This version uses Apple's ASPL Rendezvous
daemon.
%prep
%setup -q

%build
./configure --prefix=$RPM_BUILD_ROOT/usr

make RPM_OPT_FLAGS="$RPM_OPT_FLAGS"

%install
rm -rf $RPM_BUILD_ROOT
make install
mkdir -p $RPM_BUILD_ROOT/etc/rc.d/init.d
mkdir -p $RPM_BUILD_ROOT/var/cache/mt-daapd
cp contrib/mt-daapd $RPM_BUILD_ROOT/etc/rc.d/init.d
cp contrib/mt-daapd.conf $RPM_BUILD_ROOT/etc
cp contrib/mt-daapd.playlist $RPM_BUILD_ROOT/etc

%post
/sbin/chkconfig --add mt-daapd

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%config /etc/mt-daapd.conf
%config /etc/mt-daapd.playlist
/etc/rc.d/init.d/mt-daapd
/usr/sbin/mt-daapd
/usr/share/mt-daapd/*
/var/cache/mt-daapd

%doc


%changelog
* Tue Jan 18 2005 ron <ron@pedde.com>
- Update to 0.2.1, add oggvorbis

* Tue Jun 01 2004 ron <ron@pedde.com>
- Update to 0.2.0

* Mon Apr 06 2004 ron <ron@pedde.com>
- Update to 0.2.0-pre1
- Add /var/cache/mt-daapd

* Thu Jan 29 2004 ron <ron@pedde.com>
- Update to 0.1.1

* Fri Nov 14 2003 root <root@hafnium.corbey.com> 
- Initial build.


