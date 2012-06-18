Name: @PACKAGE@
Version: @VERSION@
Release: 1
Copyright: GPL
Group: System Environment/Base
Url: http://bridge.sourceforge.net
Summary: Utilities for configuring the linux ethernet bridge.
Buildroot: %{_tmppath}/%{name}-%{version}
Source: %{name}-%{version}.tar.gz

%description
This package contains utilities for configuring the linux ethernet
bridge. The linux ethernet bridge can be used for connecting multiple
ethernet devices together. The connecting is fully transparent: hosts
connected to one ethernet device see hosts connected to the other
ethernet devices directly.

Install bridge-utils if you want to use the linux ethernet bridge.

%package -n bridge-utils-devel
Summary: Utilities for configuring the linux ethernet bridge.
Group: Development/Libraries

%description -n bridge-utils-devel
The bridge-utils-devel package contains the header and object files
necessary for developing programs which use 'libbridge.a', the
interface to the linux kernel ethernet bridge. If you are developing
programs which need to configure the linux ethernet bridge, your
system needs to have these standard header and object files available
in order to create the executables.

Install bridge-utils-devel if you are going to develop programs which
will use the linux ethernet bridge interface library.

%prep
%setup -q

%build
CFLAGS="${RPM_OPT_FLAGS}" ./configure --prefix=/usr --mandir=%{_mandir}
make

%install
rm -rf %{buildroot}

mkdir -p %{buildroot}%{_sbindir}
mkdir -p %{buildroot}%{_includedir}
mkdir -p %{buildroot}%{_libdir}
mkdir -p %{buildroot}%{_mandir}/man8
install -m755 brctl/brctl %{buildroot}%{_sbindir}
gzip doc/brctl.8
install -m 644 doc/brctl.8.gz %{buildroot}%{_mandir}/man8
install -m 644 libbridge/libbridge.h %{buildroot}%{_includedir}
install -m 644 libbridge/libbridge.a %{buildroot}%{_libdir}

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%files
%defattr (-,root,root)
%doc AUTHORS COPYING doc/FAQ doc/HOWTO doc/RPM-GPG-KEY
%{_sbindir}/brctl
%{_mandir}/man8/brctl.8.gz

%files -n bridge-utils-devel
%defattr (-,root,root)
%{_includedir}/libbridge.h
%{_libdir}/libbridge.a

%changelog
* Tue May 25 2004 Stephen Hemminger <shemminger@osdl.org>
- cleanup to work for 1.0 code
- add dependency on sysfs

* Wed Nov 07 2001 Matthew Galgoci <mgalgoci@redhat.com>
- initial cleanup of spec file from net release

