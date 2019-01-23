%?mingw_package_header

Summary: shared libraries for terminal handling
Name: mingw32-ncurses6
Version: 6.0
Release: 20150808
License: X11
Group: Development/Libraries
Source: ncurses-%{version}-%{release}.tgz
# URL: http://invisible-island.net/ncurses/

BuildRequires:  mingw32-filesystem >= 95
BuildRequires:  mingw32-gcc
BuildRequires:  mingw32-binutils

BuildRequires:  mingw64-filesystem >= 95
BuildRequires:  mingw64-gcc
BuildRequires:  mingw64-binutils

%define CC_NORMAL -Wall -Wstrict-prototypes -Wmissing-prototypes -Wshadow -Wconversion
%define CC_STRICT %{CC_NORMAL} -W -Wbad-function-cast -Wcast-align -Wcast-qual -Wmissing-declarations -Wnested-externs -Wpointer-arith -Wwrite-strings -ansi -pedantic

%description -n mingw32-ncurses6
Cross-compiling support for ncurses to mingw32.

The ncurses library routines are a terminal-independent method of
updating character screens with reasonable optimization.

This package is used for testing ABI 6 with cross-compiles to MinGW.

%package -n mingw64-ncurses6
Summary:        Curses library for MinGW64

%description -n mingw64-ncurses6
Cross-compiling support for ncurses to mingw64.

The ncurses library routines are a terminal-independent method of
updating character screens with reasonable optimization.

This package is used for testing ABI 6 with cross-compiles to MinGW.

%prep

%define CFG_OPTS \\\
	--disable-echo \\\
	--disable-db-install \\\
	--disable-getcap \\\
	--disable-hard-tabs \\\
	--disable-leaks \\\
	--disable-macros \\\
	--disable-overwrite \\\
	--disable-termcap \\\
	--enable-const \\\
	--enable-ext-colors \\\
	--enable-ext-mouse \\\
	--enable-ext-putwin \\\
	--enable-interop \\\
	--enable-sp-funcs \\\
	--enable-term-driver \\\
	--enable-warnings \\\
	--enable-widec \\\
	--verbose \\\
	--with-cxx-shared \\\
	--with-develop \\\
	--with-fallbacks=unknown,rxvt \\\
	--with-shared \\\
	--with-tparm-arg=intptr_t \\\
	--with-trace \\\
	--with-xterm-kbs=DEL \\\
	--without-ada \\\
	--without-debug \\\
	--with-install-prefix=$RPM_BUILD_ROOT \\\
	--without-manpages \\\
	--without-progs \\\
	--without-tests

%define debug_package %{nil}
%setup -q -n ncurses-%{version}-%{release}

%build
mkdir BUILD-W32
pushd BUILD-W32
CFLAGS="%{CC_NORMAL}" \
CC=%{mingw32_cc} \
%mingw32_configure %{CFG_OPTS}
make
popd

mkdir BUILD-W64
pushd BUILD-W64
CFLAGS="%{CC_NORMAL}" \
CC=%{mingw64_cc} \
%mingw64_configure %{CFG_OPTS}
make
popd

%install
rm -rf $RPM_BUILD_ROOT

mkdir -p $RPM_BUILD_ROOT%{_bindir}

pushd BUILD-W32
%{mingw32_make} install.libs
for name in $RPM_BUILD_ROOT%{mingw32_bindir}/*-config; \
	do \
		base=`basename $name`; \
		ln -v $name $RPM_BUILD_ROOT%{_bindir}/%{mingw32_target}-$base; \
	done
popd

pushd BUILD-W64
%{mingw64_make} install.libs
for name in $RPM_BUILD_ROOT%{mingw64_bindir}/*-config; \
	do \
		base=`basename $name`; \
		ln -v $name $RPM_BUILD_ROOT%{_bindir}/%{mingw64_target}-$base; \
	done
popd

%clean
rm -rf $RPM_BUILD_ROOT

%files -n mingw32-ncurses6
%defattr(-,root,root,-)
%{_bindir}/%{mingw32_target}-*
%{mingw32_bindir}/*
%{mingw32_includedir}/*
%{mingw32_libdir}/*

%files -n mingw64-ncurses6
%defattr(-,root,root,-)
%{_bindir}/%{mingw64_target}-*
%{mingw64_bindir}/*
%{mingw64_includedir}/*
%{mingw64_libdir}/*

%changelog

* Sat Sep 20 2014 Thomas E. Dickey
- adjust install-rules for ncurses*-config

* Sat Aug 03 2013 Thomas E. Dickey
- initial version, using mingw-pdcurses package as a guide.
