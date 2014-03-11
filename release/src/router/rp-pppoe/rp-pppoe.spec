Summary: PPP Over Ethernet (xDSL support)
Name: rp-pppoe
Version: 3.11
%if %(%{expand:test %{_vendor} != mandrake ; echo $?})
Release: 1mdk
%else
Release: 1
%endif
License: GPL
Group: System Environment/Daemons
Source: http://www.roaringpenguin.com/penguin/pppoe/rp-pppoe-3.10.tar.gz
Url: http://www.roaringpenguin.com/pppoe/
Packager: David F. Skoll <dfs@roaringpenguin.com>
BuildRoot: /tmp/pppoe-build
Vendor: Roaring Penguin Software Inc.
Requires: ppp >= 2.3.7

# LIC: GPL
%description
PPPoE (Point-to-Point Protocol over Ethernet) is a protocol used by
many DSL Internet Service Providers. Roaring Penguin has a free
client for Linux systems to connect to PPPoE service providers.

The client is a user-mode program and does not require any kernel
modifications. It is fully compliant with RFC 2516, the official PPPoE
specification.

%prep
%setup
cd src
./configure --mandir=%{_mandir}

%build
cd src
make
cd ../gui
make

%install
umask 022
cd src
make install DESTDIR=$RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/etc/init.d
test -f $RPM_BUILD_ROOT/etc/init.d/pppoe || cp ../scripts/pppoe-init $RPM_BUILD_ROOT/etc/init.d/pppoe
chmod 755 $RPM_BUILD_ROOT/etc/init.d/pppoe
cd ../gui
make install DESTDIR=$RPM_BUILD_ROOT
rm -f $RPM_BUILD_ROOT/etc/ppp/pppoe.conf-3.10
rm -f $RPM_BUILD_ROOT/etc/ppp/firewall-masq-3.10
rm -f $RPM_BUILD_ROOT/etc/ppp/firewall-standalone-3.10
rm -f $RPM_BUILD_ROOT/etc/ppp/pppoe-server-options-example

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc doc/CHANGES doc/HOW-TO-CONNECT doc/LICENSE doc/KERNEL-MODE-PPPOE README SERVPOET
%config(noreplace) /etc/ppp/pppoe.conf
%config(noreplace) /etc/ppp/pppoe-server-options
%config(noreplace) /etc/ppp/firewall-masq
%config(noreplace) /etc/ppp/firewall-standalone
/etc/ppp/plugins/*
/usr/sbin/pppoe
/usr/sbin/pppoe-server
/usr/sbin/pppoe-sniff
/usr/sbin/pppoe-relay
/usr/sbin/pppoe-connect
/usr/sbin/pppoe-start
/usr/sbin/pppoe-stop
/usr/sbin/pppoe-setup
/usr/sbin/pppoe-status
%{_mandir}/man5/pppoe.conf.5*
%{_mandir}/man8/pppoe.8*
%{_mandir}/man8/pppoe-server.8*
%{_mandir}/man8/pppoe-relay.8*
%{_mandir}/man8/pppoe-sniff.8*
%{_mandir}/man8/pppoe-connect.8*
%{_mandir}/man8/pppoe-start.8*
%{_mandir}/man8/pppoe-stop.8*
%{_mandir}/man8/pppoe-status.8*
%{_mandir}/man8/pppoe-setup.8*
/etc/init.d/pppoe
%package gui
Summary: Tk PPP Over Ethernet Client (xDSL support)
Group: System Environment/Daemons
Requires: rp-pppoe
Requires: tk

%description gui
This is a graphical wrapper around the rp-pppoe PPPoE client.  PPPoE is
a protocol used by many DSL Internet Service Providers.

%post gui
# Install entry in KDE menu
if test -n "$KDEDIR" ; then
    mkdir -p "$KDEDIR/share/applnk/Internet"
    cat <<EOF > "$KDEDIR/share/applnk/Internet/tkpppoe.kdelnk"
# KDE Config File
[KDE Desktop Entry]
Name=TkPPPoE
Comment=Start/Stop PPPoE connections
Exec=tkpppoe
Terminal=0
Type=Application
EOF
fi

# Install entry in GNOME menus
GNOMEDIR=`gnome-config --datadir 2>/dev/null`
if test -n "$GNOMEDIR" ; then
    mkdir -p "$GNOMEDIR/gnome/apps/Internet"
cat <<EOF > "$GNOMEDIR/gnome/apps/Internet/tkpppoe.desktop"
[Desktop Entry]
Name=TkPPPoE
Comment=Start/Stop PPPoE connections
Exec=tkpppoe
Terminal=0
Type=Application
EOF
fi

%postun gui
# Remove KDE menu entry
if test -n "$KDEDIR" ; then
    rm -f "$KDEDIR/share/applnk/Internet/tkpppoe.kdelnk"
fi

# Remove GNOME menu entry
GNOMEDIR=`gnome-config --datadir 2>/dev/null`
if test -n "$GNOMEDIR" ; then
    rm -f "$GNOMEDIR/gnome/apps/Internet/tkpppoe.desktop"
fi

%files gui
%defattr(-,root,root)
%dir /etc/ppp/rp-pppoe-gui
/usr/sbin/pppoe-wrapper
/usr/bin/tkpppoe
%{_mandir}/man1/tkpppoe.1*
%{_mandir}/man1/pppoe-wrapper.1*
/usr/share/tkpppoe/tkpppoe.html
/usr/share/tkpppoe/mainwin-busy.png
/usr/share/tkpppoe/mainwin-nonroot.png
/usr/share/tkpppoe/mainwin.png
/usr/share/tkpppoe/props-advanced.png
/usr/share/tkpppoe/props-basic.png
/usr/share/tkpppoe/props-nic.png
/usr/share/tkpppoe/props-options.png
/usr/share/tkpppoe/en.msg
/usr/share/tkpppoe/ja.msg

%changelog
* Thu Jul 21 2001 Shigechika AIKAWA <shige@cin.nihon-u.ac.jp>
- merged rp-pppoe.spec and rp-pppoe-gui.spec
