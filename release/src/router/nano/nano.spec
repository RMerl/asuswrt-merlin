%define name	nano
%define version	2.8.1
%define release	1

Name		: %{name}
Version		: %{version}
Release		: %{release}
Summary		: a user-friendly editor, a Pico clone with enhancements

License		: GPLv3+
Group		: Applications/Editors
URL		: https://nano-editor.org/
Source		: https://nano-editor.org/dist/v2.6/%{name}-%{version}.tar.gz

BuildRoot	: %{_tmppath}/%{name}-%{version}-root
BuildRequires	: autoconf, automake, gettext-devel, ncurses-devel, texinfo
Requires(post)	: info
Requires(preun)	: info

%description
GNU nano is a small and friendly text editor.  It aims to emulate the
Pico text editor while also offering several enhancements.

%prep
%setup -q

%build
%configure
make

%install
make DESTDIR="%{buildroot}" install
#ln -s nano %{buildroot}/%{_bindir}/pico
rm -f %{buildroot}/%{_infodir}/dir

%post
/sbin/install-info %{_infodir}/%{name}.info %{_infodir}/dir || :

%preun
if [ $1 = 0 ] ; then
/sbin/install-info --delete %{_infodir}/%{name}.info %{_infodir}/dir || :
fi

%files
%defattr(-,root,root)
%doc AUTHORS COPYING ChangeLog INSTALL NEWS README THANKS TODO doc/faq.html doc/sample.nanorc
%{_bindir}/*
%{_docdir}/nano/*
%{_mandir}/man*/*
%{_mandir}/fr/man*/*
%{_infodir}/nano.info*
%{_datadir}/locale/*/LC_MESSAGES/nano.mo
%{_datadir}/nano/*
