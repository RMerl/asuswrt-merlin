Name:           umem
Version:        1.0.1
Release:        2%{?dist}
Summary:        Port of Solaris's slab allocator.

Group:          System Environment/Libraries
License:        CDDL
URL:            https://labs.omniti.com/trac/portableumem/
Source0:        %{name}-%{version}.tar.bz2
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:  autoconf >= 2.50
BuildRequires:  automake >= 1.4
BuildRequires:  libtool >= 1.4.2
BuildRequires:  doxygen
BuildRequires:  gcc
BuildRequires:  binutils
BuildRequires:  make
BuildRequires:  mktemp


%description
This a port of Solaris's slab allocator, libumem, to Linux.

"A slab allocator is a cache management structure for efficient use
of [...] memory. [...] It is targeted for use of many small pieces
of memory chunks. By managing small memory chunks in the units
called slabs, this mechanism enables lower fragmentation, fast allocation,
and reclaming memory." (Description sourced from Wikipedia.)


%prep
%setup -q


%build
%configure
%{__make}
%{__make} check
%{__make} html


%install
rm -rf $RPM_BUILD_ROOT
%makeinstall

# Remove the libtool files -- we don't want them.
find $RPM_BUILD_ROOT%{_libdir} -name '*.la' | xargs rm -fv

# Remove the symlink to the SONAME. Let ldconfig manage that.
rm -fv $RPM_BUILD_ROOT%{_libdir}/*.so.[0-9]

# Build the pkgconfig configurations.
mkdir -p $RPM_BUILD_ROOT%{_libdir}/pkgconfig

cat<<EOT >$RPM_BUILD_ROOT%{_libdir}/pkgconfig/%{name}-%{version}.pc
prefix=%{_prefix}
exec_prefix=%{_exec_prefix}
libdir=%{_libdir}
includedir=%{_includedir}

Name: %{name}
Version: %{version}
Description: Port of Solaris's slab allocator.
URL: https://labs.omniti.com/trac/portableumem/
Requires:
Libs: -L\${libdir} -lumem
Cflags:
EOT

cat<<EOT >$RPM_BUILD_ROOT%{_libdir}/pkgconfig/%{name}-malloc-%{version}.pc
prefix=%{_prefix}
exec_prefix=%{_exec_prefix}
libdir=%{_libdir}
includedir=%{_includedir}

Name: %{name}
Version: %{version}
Description: Port of Solaris's slab allocator. Libc malloc replacement.
URL: https://labs.omniti.com/trac/portableumem/
Requires:
Libs: -L\${libdir} -lumem_malloc
Cflags:
EOT


%clean
rm -rf $RPM_BUILD_ROOT


%pre
/sbin/ldconfig


%post
/sbin/ldconfig


%files
%defattr(-,root,root,-)
%doc AUTHORS COPYING COPYRIGHT INSTALL NEWS OPENSOLARIS.LICENSE README
%{_libdir}/*.so.*


%package devel

Summary: Port of Solaris's slab allocator.

Group: Development/Libraries

Requires: pkgconfig


%description devel

This contains the libraries and header files for using this port
of Solaris's slab allocator, libumem, to Linux.


%files devel
%defattr(-,root,root,-)
%doc AUTHORS COPYING COPYRIGHT INSTALL NEWS OPENSOLARIS.LICENSE README TODO
%doc docs/html
%{_includedir}/*.h
%{_includedir}/sys/*.h
%{_libdir}/*.so
%{_libdir}/*.a
%{_mandir}/man*/*
%{_libdir}/pkgconfig/*.pc
