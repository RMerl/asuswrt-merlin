#
# "$Id: lsb-samba.spec,v 1.2 2001/07/03 01:01:12 jra Exp $"
#
#   Linux Standards Based RPM "spec" file for SAMBA.
#

Summary: SAMBA
Name: lsb-samba
Version: 2.2.1
Release: 0
Copyright: GPL
Group: System Environment/Daemons
Source: ftp://ftp.samba.org/pub/samba/samba-%{version}.tar.gz
Url: http://www.samba.org
Packager: Michael Sweet <mike@easysw.com>
Vendor: SAMBA Team

# Require the "lsb" package, which guarantees LSB compliance.
Requires: lsb

# use BuildRoot so as not to disturb the version already installed
BuildRoot: /var/tmp/%{name}-root

%description

%prep
%setup

%build
export LDFLAGS="-L/usr/lib/lsb --dynamic-linker=/lib/ld-lsb.so.1"

./configure --with-fhs  --prefix=/usr --sysconfdir=/etc \
            --sharedstatedir=/var --datadir=/usr/share \
            --with-configdir=/etc/samba \
            --with-swatdir=/usr/share/samba/swat

# If we got this far, all prerequisite libraries must be here.
make

%install
# Make sure the RPM_BUILD_ROOT directory exists.
rm -rf $RPM_BUILD_ROOT
mkdir $RPM_BUILD_ROOT

make \
	BASEDIR=$RPM_BUILD_ROOT/usr \
	BINDIR=$RPM_BUILD_ROOT/usr/bin \
	CODEPAGEDIR=$RPM_BUILD_ROOT/usr/share/samba/codepages \
	CONFIGDIR=$RPM_BUILD_ROOT/etc/samba \
	INCLUDEDIR=$RPM_BUILD_ROOT/usr/include \
	LIBDIR=$RPM_BUILD_ROOT/usr/lib \
	LOCKDIR=$RPM_BUILD_ROOT/var/lock/samba \
	LOGFILEBASE=$RPM_BUILD_ROOT/var/log/samba \
	MANDIR=$RPM_BUILD_ROOT/usr/share/man \
	SBINDIR=$RPM_BUILD_ROOT/usr/sbin \
	SWATDIR=$RPM_BUILD_ROOT/usr/share/samba/swat \
	VARDIR=$RPM_BUILD_ROOT/var \
	install

mkdir -p $RPM_BUILD_ROOT/etc/init.d
install -m 700 packaging/LSB/samba.sh /etc/init.d/samba

mkdir -p $RPM_BUILD_ROOT/etc/samba
install -m 644 packaging/LSB/smb.conf /etc/samba

mkdir -p $RPM_BUILD_ROOT/etc/xinetd.d
install -m 644 packaging/LSB/samba.xinetd /etc/xinetd.d/samba

%post
/usr/lib/lsb/install_initd /etc/init.d/samba

%preun
/usr/lib/lsb/remove_initd /etc/init.d/samba

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%dir /etc/init.d
/etc/init.d/samba
%dir /etc/samba
%config(noreplace) /etc/samba/smb.conf
%dir /etc/samba/private
%dir /etc/xinetd.d
%config(noreplace) /etc/xinetd.d/samba
%dir /usr/bin
/usr/bin/*
%dir /usr/sbin
/usr/sbin/*
%dir /usr/share/man
/usr/share/man/*
%dir /usr/share/samba
/usr/share/samba/*
%dir /var/lock/samba
%dir /var/log/samba

#
# End of "$Id: lsb-samba.spec,v 1.2 2001/07/03 01:01:12 jra Exp $".
#
