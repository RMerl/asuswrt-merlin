# $Source: /opt/cvs/samba/smbldap-tools/smbldap-tools.spec,v $
%define version 0.9.2
%define release 1
%define name 	smbldap-tools
%define realname  smbldap-tools
%define _prefix /opt/IDEALX
%define _sysconfdir /etc/opt/IDEALX

Summary:       User & Group administration tools for Samba/LDAP
Name: 		%{name}
version: 	%{version}
Release: 	%{release}
Group: 		System Environment/Base
License: 	GPL
Vendor:		IDEALX
URL:		http://samba.idealx.org/
Packager:	Jerome Tournier <jerome.tournier@idealx.com>
Source0: 	smbldap-tools-%{version}.tgz
BuildRoot: 	/%{_tmppath}/%{name}
BuildRequires:	perl >= 5.6
Requires:	perl >= 5.6, samba
Prefix:		%{_prefix}
BuildArch:	noarch

%description
Smbldap-tools is a set of perl scripts designed to manage user and group 
accounts stored in an LDAP directory. They can be used both by users and 
administrators of Linux systems: 
. administrators can perform users and groups management operations, in a 
  way similar to the standard useradd or groupmod commands
. users can change their LDAP password from the command line

These scripts are developed and maintained by IDEALX for the Samba project.

Scripts are described in the Smbldap-tools User Manual
(http://samba.idealx.org/smbldap-tools.en.html) which also give command
line examples.
You can download the latest version on IDEALX web site
(http://samba.idealx.org/dist/).
Comments and/or questions can be sent to the smbldap-tools mailing list
(http://lists.idealx.org/lists/samba).

%prep
%setup -q

%build
sed -i "s,/etc/opt/IDEALX/smbldap-tools/,%{_sysconfdir}/smbldap-tools/,g" smbldap_tools.pm
sed -i "s,/etc/opt/IDEALX/smbldap-tools/,%{_sysconfdir}/smbldap-tools/,g" configure.pl
sed -i "s,/etc/opt/IDEALX/,%{_sysconfdir}/,g" smbldap.conf


%install
%{__rm} -rf %{buildroot}
#%makeinstall
#mkdir -p $RPM_BUILD_ROOT/{etc/smbldap-tools,opt/IDEALX/sbin}
mkdir -p $RPM_BUILD_ROOT/%_sysconfdir/smbldap-tools
mkdir -p $RPM_BUILD_ROOT/%_prefix/sbin/

for i in smbldap-*
do
	install $i $RPM_BUILD_ROOT/%prefix/sbin/$i
done
install configure.pl $RPM_BUILD_ROOT/%prefix/sbin/configure.pl
cp -a smbldap.conf smbldap_bind.conf $RPM_BUILD_ROOT/%{_sysconfdir}/smbldap-tools/
cp -a smbldap_tools.pm $RPM_BUILD_ROOT/%{prefix}/sbin
rm $RPM_BUILD_ROOT/%{prefix}/sbin/smbldap-tools.spec


%clean
if [ -n "$RPM_BUILD_ROOT" ] ; then
   [ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT
fi

%pre
files=`ls %{_prefix}/sbin/smbldap*.pl 2>/dev/null`
if [ "$files" != "" ];
then
	echo "WARNING: new scripts do not have any .pl extension"
	echo "         You need to update the smb.conf file"
fi

%post
# from smbldap-tools-0.8-2, libraries are loaded with the FindBin perl package
if [ -f /usr/lib/perl5/site_perl/smbldap_tools.pm ];
then
	rm -f /usr/lib/perl5/site_perl/smbldap_tools.pm
fi
if [ -f /usr/lib/perl5/site_perl/smbldap_conf.pm ];
then
	rm -f /usr/lib/perl5/site_perl/smbldap_conf.pm
fi

if [ ! -n `grep with_slappasswd %{_sysconfdir}/smbldap-tools/smbldap.conf | grep -v "^#"` ];
then
        echo "Check if you have the with_slappasswd parameter defined"
	echo "in smbldap.conf file (see the INSTALL file)"
fi

%files
%defattr(-,root,root)
%{prefix}/sbin/configure.pl
%{prefix}/sbin/smbldap-groupadd
%{prefix}/sbin/smbldap-groupdel
%{prefix}/sbin/smbldap-groupmod
%{prefix}/sbin/smbldap-groupshow
%{prefix}/sbin/smbldap-passwd
%{prefix}/sbin/smbldap-populate
%{prefix}/sbin/smbldap-useradd
%{prefix}/sbin/smbldap-userdel
%{prefix}/sbin/smbldap-usermod
%{prefix}/sbin/smbldap-userinfo
%{prefix}/sbin/smbldap-usershow
%{prefix}/sbin/smbldap_tools.pm
%doc CONTRIBUTORS COPYING ChangeLog FILES INFRA README INSTALL TODO
%doc smb.conf smbldap.conf smbldap_bind.conf
%doc doc/smbldap-*
%doc doc/html
%defattr(644,root,root)
%config(noreplace) %{_sysconfdir}/smbldap-tools/smbldap.conf
%defattr(600,root,root)
%config(noreplace) %{_sysconfdir}/smbldap-tools/smbldap_bind.conf

%changelog
* Fri Nov 28 2003 Jerome Tournier <jerome.tournier@idealx.com> 0.8.2-1
- see Changelog file for updates in scripts

