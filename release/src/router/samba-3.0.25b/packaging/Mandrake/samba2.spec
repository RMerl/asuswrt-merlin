# Note that this file exists in Mandrake packaging cvs (as samba.spec)
# and samba cvs (as packaging/Mandrake/samba2.spec.tmpl).
# Keep in mind that any changes should take both locations into account
# Considerable effort has gone into making this possible, so that only
# one spec file is maintained, please don't break it.
# It should be possible, without any changes to this file, to build
# binary packages on most recent Mandrake releases:
# 1)from official source releases, using 'cd packaging/Mandrake; sh makerpms.sh'
# 2)from cvs snapshots, using 'cd packaging/Mandrake; sh makerpms-cvs.sh <ver>'
# 3)using official source releases and updated Mandrake packaging, by
#   'rpm -ba samba.spec'
# As such, any sources or patches used in a build from a samba release or
# cvs should be submitted for inclusion in samba cvs.

%define pkg_name	samba
%define ver 		3.0.8
%define drel		1
%define	subrel		1
%define vscanver 	0.3.5
%define libsmbmajor 	0
%{?!mdkversion: %define mdkversion %(perl -pe '/(\d+)\.(\d)\.?(\d)?/; $_="$1$2".($3||0)' /etc/mandrake-release)}
%define rel		%(case `hostname` in (n?.mandrakesoft.com) echo %drel;;(*) echo $[%drel-1].%subrel.$[%mdkversion/10];;esac)mdk
%{?_with_stable_on_cluster: %define rel %(echo $[%{drel}-1].%subrel.$[%{mdkversion}/10]mdk)}
%{?_with_official: %define rel %drel}

%{!?lib: %global lib lib}
%{!?mklibname: %global mklibname(ds) %lib%{1}%{?2:%{2}}%{?3:_%{3}}%{-s:-static}%{-d:-devel}}

%define libname %mklibname smbclient %libsmbmajor

# Version and release replaced by samba-team at release from samba cvs
%define pversion 3.0.13
%define prelease 1

#Check to see if p(version|release) has been replaced (1 if replaced)
%define have_pversion %(if [ "%pversion" = `echo "pversion" |tr '[:lower:]' '[:upper:]'` ];then echo 0; else echo 1; fi)
%define have_prelease %(if [ "%prelease" = `echo "prelease" |tr '[:lower:]' '[:upper:]'` ];then echo 0; else echo 1; fi)

%if %have_pversion
%define source_ver 	%{pversion}
# Don't abort for stupid reasons on builds from tarballs:
%global	_unpackaged_files_terminate_build	0
%global	_missing_doc_files_terminate_build	0
%else
%define source_ver 	%{ver}
%endif

# We might have a prerelease:
%define have_pre %(echo %source_ver|awk '{p=0} /[a-z,A-Z][a-z,A-Z]/ {p=1} {print p}')
%if %have_pre
%define pre_ver %(perl -e '$name="%source_ver"; print ($name =~ /(.*?)[a-z]/);')
%define pre_pre %(echo %source_ver|sed -e 's/%pre_ver//g')
%endif

# Check to see if we are running a build from a tarball release from samba.org
# (%have_pversion) If so, disable vscan, unless explicitly requested
# (--with vscan).
%define build_vscan 	1
%if %have_pversion
%define build_vscan 	0
%{?_with_vscan: %define build_vscan 1}
%endif

# We now do detection of the Mandrake release we are building on:
%define build_mdk82 %(if [ `awk '{print $4}' /etc/mandrake-release` = 8.2 ];then echo 1; else echo 0; fi)
%define build_mdk81 %(if [ `awk '{print $4}' /etc/mandrake-release` = 8.1 ];then echo 1; else echo 0; fi)
%define build_mdk80 %(if [ `awk '{print $4}' /etc/mandrake-release` = 8.0 ];then echo 1; else echo 0; fi)
%define build_mdk72 %(if [ `awk '{print $4}' /etc/mandrake-release` = 7.2 ];then echo 1; else echo 0; fi)
%define build_non_default 0

# Default options
%define build_alternatives	0
%define build_system	0
%define build_acl 	1
%define build_winbind 	1
%define build_wins 	1
%define build_ldap 	0
%define build_ads	1
%define build_scanners	0
# CUPS supports functionality for 'printcap name = cups' (9.0 and later):
%define build_cupspc	0
# %_{pre,postun}_service are provided by rpm-helper in 9.0 and later
%define have_rpmhelper	1

# Set defaults for each version
%if %mdkversion >= 1000
%define build_system	1
%endif

%if %mdkversion >= 920
%define build_alternatives	1
%endif

%if %mdkversion >= 910
%define build_cupspc	1
%endif

%if %build_mdk82
%define have_rpmhelper	0
%endif

%if %build_mdk81
%define build_winbind	0
%define build_wins	0
%define have_rpmhelper	0
%endif

%if %build_mdk80
%define build_acl	0
%define build_winbind	0
%define build_wins	0
%define build_ads	0
%define have_rpmhelper	1
%endif

%if %build_mdk72
%define build_acl	0
%define build_winbind	0
%define build_wins	0
%define build_ads	0
%define have_rpmhelper	1
%endif


# Allow commandline option overrides (borrowed from Vince's qmail srpm):
# To use it, do rpm [-ba|--rebuild] --with 'xxx'
# Check if the rpm was built with the defaults, otherwise we inform the user
%define build_non_default 0
%{?_with_system: %global build_system 1}
%{?_without_system: %global build_system 0}
%{?_with_acl: %global build_acl 1}
%{?_with_acl: %global build_non_default 1}
%{?_without_acl: %global build_acl 0}
%{?_without_acl: %global build_non_default 1}
%{?_with_winbind: %global build_winbind 1}
%{?_with_winbind: %global build_non_default 1}
%{?_without_winbind: %global build_winbind 0}
%{?_without_winbind: %global build_non_default 1}
%{?_with_wins: %global build_wins 1}
%{?_with_wins: %global build_non_default 1}
%{?_without_wins: %global build_wins 0}
%{?_without_wins: %global build_non_default 1}
%{?_with_ldap: %global build_ldap 1}
%{?_with_ldap: %global build_non_default 1}
%{?_without_ldap: %global build_ldap 0}
%{?_without_ldap: %global build_non_default 1}
%{?_with_ads: %global build_ads 1}
%{?_with_ads: %global build_non_default 1}
%{?_without_ads: %global build_ads 0}
%{?_without_ads: %global build_non_default 1}
%{?_with_scanners: %global build_scanners 1}
%{?_with_scanners: %global build_non_default 1}
%{?_without_scanners: %global build_scanners 0}
%{?_without_scanners: %global build_non_default 1}
%{?_with_vscan: %global build_vscan 1}
%{?_with_vscan: %global build_non_default 1}
%{?_without_vscan: %global build_vscan 0}
%{?_without_vscan: %global build_non_default 1}

# As if that weren't enough, we're going to try building with antivirus
# support as an option also
%global build_clamav 	0
%global build_fprot 	0
%global build_fsav 	0
%global build_icap 	0
%global build_kaspersky 0
%global build_mks 	0
%global build_nai 	0
%global build_openav	0
%global build_sophos 	0
%global build_symantec 	0
%global build_trend	0
%if %build_vscan
# These we build by default
%global build_clamav 	1
%global build_icap 	1
%endif
%if %build_vscan && %build_scanners
# These scanners are built if scanners are selected
# symantec requires their library present and must be selected 
# individually
%global build_fprot 	1
%global build_fsav 	1
%global build_kaspersky 1
%global build_mks 	1
%global build_nai 	1
%global build_openav	1
%global build_sophos 	1
%global build_trend 	1
%endif
%if %build_vscan
%{?_with_fprot: %{expand: %%global build_fprot 1}}
%{?_with_kaspersky: %{expand: %%global build_kaspersky 1}}
%{?_with_mks: %{expand: %%global build_mks 1}}
%{?_with_openav: %{expand: %%global build_openav 1}}
%{?_with_sophos: %{expand: %%global build_sophos 1}}
#%{?_with_symantec: %{expand: %%global build_symantec 1}}
%{?_with_trend: %{expand: %%global build_trend 1}}
%global vscandir samba-vscan-%{vscanver}
%endif
%global vfsdir examples.bin/VFS

#Standard texts for descriptions:
%define message_bugzilla() %(echo -e -n "Please file bug reports for this package at Mandrake bugzilla \\n(http://qa.mandrakesoft.com) under the product name %{1}")
%define message_system %(echo -e -n "NOTE: These packages of samba-%{version}, are provided, parallel installable\\nwith samba-2.2.x, to allow easy migration from samba-2.2.x to samba-%{version},\\nbut are not officially supported")

#check gcc version to disable some optimisations on gcc-3.3.1
%define gcc331 %(gcc -dumpversion|awk '{if ($1>3.3) print 1; else print 0}')

#Define sets of binaries that we can use in globs and loops:
%global commonbin net,ntlm_auth,rpcclient,smbcacls,smbcquotas,smbpasswd,smbtree,testparm,testprns

%global serverbin 	editreg,pdbedit,profiles,smbcontrol,smbstatus,tdbbackup,tdbdump
%global serversbin nmbd,samba,smbd,mkntpwd

%global clientbin 	findsmb,nmblookup,smbclient,smbmnt,smbmount,smbprint,smbspool,smbtar,smbumount,smbget
%global client_bin 	mount.cifs
%global client_sbin 	mount.smb,mount.smbfs

%global testbin 	debug2html,smbtorture,msgtest,masktest,locktest,locktest2,nsstest,vfstest

%ifarch alpha
%define build_expsam xml
%else
%define build_expsam mysql,xml,pgsql
%endif

#Workaround missing macros in 8.x:
%{!?perl_vendorlib: %{expand: %%global perl_vendorlib %{perl_sitearch}/../}}

# Determine whether this is the system samba or not.
%if %build_system
%define samba_major	%{nil}
%else
%define samba_major	3
%endif
# alternatives_major is %{nil} if we aren't system and not using alternatives
%if !%build_system || %build_alternatives
%define alternative_major 3
%else
%define alternative_major %{nil}
%endif

Summary: Samba SMB server.
Name: %{pkg_name}%{samba_major}

%if %have_pre
Version: %{pre_ver}
%else
Version: %{source_ver}
%endif

%if %have_prelease && !%have_pre
Release: 0.%{prelease}.%{rel}
%endif
%if %have_prelease && %have_pre
Release: 0.%{pre_pre}.%{rel}
%endif
%if !%have_prelease && !%have_pre
Release: %{rel}
%endif
%if !%have_prelease && %have_pre
Release: 0.%{pre_pre}.%{rel}
%endif

License: GPL
Group: System/Servers
Source: ftp://samba.org/pub/samba/samba-%{source_ver}.tar.bz2
URL:	http://www.samba.org
Source1: samba.log
Source3: samba.xinetd
Source4: swat_48.png.bz2
Source5: swat_32.png.bz2
Source6: swat_16.png.bz2
Source7: README.%{name}-mandrake-rpm
%if %build_vscan
Source8: samba-vscan-%{vscanver}.tar.bz2
%endif
%if %build_vscan && %mdkversion >= 920
BuildRequires: file-devel
%endif
Source10: samba-print-pdf.sh.bz2
Source11: smb-migrate.bz2
Patch1: smbw.patch.bz2
Patch4: samba-3.0-smbmount-sbin.patch.bz2
Patch5:	samba-3.0.2a-smbldap-config.patch.bz2
%if !%have_pversion
# Version specific patches: current version
Patch7: samba-3.0.5-lib64.patch.bz2
Patch9:	samba-3.0.6-smbmount-unixext.patch.bz2
Patch11: samba-3.0.7-mandrake-packaging.patch.bz2
%else
# Version specific patches: upcoming version
Patch8:	samba-3.0.6-revert-libsmbclient-move.patch.bz2
%endif
# Limbo patches (applied to prereleases, but not preleases, ie destined for
# samba CVS)
%if %have_pversion && %have_pre
%endif
Requires: pam >= 0.64, samba-common = %{version}
BuildRequires: pam-devel readline-devel libncurses-devel popt-devel
BuildRequires: libxml2-devel postgresql-devel
%ifnarch alpha
BuildRequires: mysql-devel
%endif
%if %build_acl
BuildRequires: libacl-devel
%endif
%if %build_mdk72
BuildRequires: cups-devel
%else
BuildRequires: libcups-devel
%endif
BuildRequires: libldap-devel
%if %build_ads
BuildRequires: libldap-devel krb5-devel
%endif
BuildRoot: %{_tmppath}/%{name}-%{version}-root
Prefix: /usr
Prereq: /sbin/chkconfig /bin/mktemp /usr/bin/killall
Prereq: fileutils sed /bin/grep

%description
Samba provides an SMB server which can be used to provide
network services to SMB (sometimes called "Lan Manager")
clients, including various versions of MS Windows, OS/2,
and other Linux machines. Samba also provides some SMB
clients, which complement the built-in SMB filesystem
in Linux. Samba uses NetBIOS over TCP/IP (NetBT) protocols
and does NOT need NetBEUI (Microsoft Raw NetBIOS frame)
protocol.

Samba-3.0 features working NT Domain Control capability and
includes the SWAT (Samba Web Administration Tool) that
allows samba's smb.conf file to be remotely managed using your
favourite web browser. For the time being this is being
enabled on TCP port 901 via xinetd. SWAT is now included in
it's own subpackage, samba-swat.

Please refer to the WHATSNEW.txt document for fixup information.
This binary release includes encrypted password support.

Please read the smb.conf file and ENCRYPTION.txt in the
docs directory for implementation details.
%if %have_pversion
%message_bugzilla samba3
%endif 
%if !%build_system
%message_system
%endif
%if %build_non_default
WARNING: This RPM was built with command-line options. Please
see README.%{name}-mandrake-rpm in the documentation for
more information.
%endif

%package server
URL:	http://www.samba.org
Summary: Samba (SMB) server programs.
Requires: %{name}-common = %{version}
%if %have_rpmhelper
PreReq:		rpm-helper
%endif
Group: Networking/Other
%if %build_system
Provides: samba
Obsoletes: samba
Provides:  samba-server-ldap
Obsoletes: samba-server-ldap
Provides:  samba3-server
Obsoletes: samba3-server
%else
#Provides: samba-server
%endif

%description server
Samba-server provides a SMB server which can be used to provide
network services to SMB (sometimes called "Lan Manager")
clients. Samba uses NetBIOS over TCP/IP (NetBT) protocols
and does NOT need NetBEUI (Microsoft Raw NetBIOS frame)
protocol.

Samba-3.0 features working NT Domain Control capability and
includes the SWAT (Samba Web Administration Tool) that
allows samba's smb.conf file to be remotely managed using your
favourite web browser. For the time being this is being
enabled on TCP port 901 via xinetd. SWAT is now included in
it's own subpackage, samba-swat.

Please refer to the WHATSNEW.txt document for fixup information.
This binary release includes encrypted password support.

Please read the smb.conf file and ENCRYPTION.txt in the
docs directory for implementation details.
%if %have_pversion
%message_bugzilla samba3-server
%endif
%if !%build_system
%message_system
%endif

%package client
URL:	http://www.samba.org
Summary: Samba (SMB) client programs.
Group: Networking/Other
Requires: %{name}-common = %{version}
%if %build_alternatives
#Conflicts:	samba-client < 2.2.8a-9mdk
%endif
%if %build_system
Provides:  samba3-client
Obsoletes: samba3-client
Obsoletes: smbfs
%else
#Provides: samba-client
%endif
%if !%build_system && %build_alternatives
Provides: samba-client
%endif

%description client
Samba-client provides some SMB clients, which complement the built-in
SMB filesystem in Linux. These allow the accessing of SMB shares, and
printing to SMB printers.
%if %have_pversion
%message_bugzilla samba3-client
%endif
%if !%build_system
%message_system
%endif

%package common
URL:	http://www.samba.org
Summary: Files used by both Samba servers and clients.
Group: System/Servers
%if %build_system
Provides:  samba-common-ldap
Obsoletes: samba-common-ldap
Provides:  samba3-common
Obsoletes: samba3-common
%else
#Provides: samba-common
%endif

%description common
Samba-common provides files necessary for both the server and client
packages of Samba.
%if %have_pversion
%message_bugzilla samba3-common
%endif
%if !%build_system
%message_system
%endif

%package doc
URL:	http://www.samba.org
Summary: Documentation for Samba servers and clients.
Group: System/Servers
Requires: %{name}-common = %{version}
%if %build_system
Obsoletes: samba3-doc
Provides:  samba3-doc
%else
#Provides: samba-doc
%endif

%description doc
Samba-doc provides documentation files for both the server and client
packages of Samba.
%if %have_pversion
%message_bugzilla samba3-doc
%endif
%if !%build_system
%message_system
%endif

%package swat
URL:	http://www.samba.org
Summary: The Samba Web Administration Tool.
Requires: %{name}-server = %{version}
Requires: xinetd
Group: System/Servers
%if %build_system
Provides:  samba-swat-ldap
Obsoletes: samba-swat-ldap
Provides:  samba3-swat
Obsoletes: samba3-swat
%else
#Provides: samba-swat
%endif

%description swat
SWAT (the Samba Web Administration Tool) allows samba's smb.conf file
to be remotely managed using your favourite web browser. For the time
being this is being enabled on TCP port 901 via xinetd. Note that
SWAT does not use SSL encryption, nor does it preserve comments in
your smb.conf file. Webmin uses SSL encryption by default, and
preserves comments in configuration files, even if it does not display
them, and is therefore the preferred method for remotely managing
Samba.
%if %have_pversion
%message_bugzilla samba3-swat
%endif
%if !%build_system
%message_system
%endif

%if %build_winbind
%package winbind
URL:	http://www.samba.org
Summary: Samba-winbind daemon, utilities and documentation
Group: System/Servers
Requires: %{name}-common = %{version}
%endif
%if %build_winbind && !%build_system
Conflicts: samba-winbind
%endif
%if %build_winbind
%description winbind
Provides the winbind daemon and testing tools to allow authentication 
and group/user enumeration from a Windows or Samba domain controller.
%endif
%if %have_pversion
%message_bugzilla samba3-winbind
%endif
%if !%build_system
%message_system
%endif

%if %build_wins
%package -n nss_wins%{samba_major}
URL:	http://www.samba.org
Summary: Name Service Switch service for WINS
Group: System/Servers
Requires: %{name}-common = %{version}
PreReq: glibc
%endif
%if %build_wins && !%build_system
Conflicts: nss_wins
%endif
%if %build_wins
%description -n nss_wins%{samba_major}
Provides the libnss_wins shared library which resolves NetBIOS names to 
IP addresses.
%endif
%if %have_pversion
%message_bugzilla nss_wins3
%endif
%if !%build_system
%message_system
%endif

%if %{?_with_test:1}%{!?_with_test:0}
%package test
URL:	http://www.samba.org
Summary: Debugging and benchmarking tools for samba
Group: System/Servers
Requires: %{name}-common = %{version}
%endif
%if %build_system && %{?_with_test:1}%{!?_with_test:0}
Provides:  samba3-test samba3-debug
Obsoletes: samba3-test samba3-debug
%endif
%if !%build_system && %{?_with_test:1}%{!?_with_test:0}
Provides: samba-test samba3-debug
Obsoletes: samba3-debug
%endif
%if %{?_with_test:1}%{!?_with_test:0}

%description test
This package provides tools for benchmarking samba, and debugging
the correct operation of tools against smb servers.
%endif

%if %build_system
%package -n %{libname}
URL:		http://www.samba.org
Summary: 	SMB Client Library
Group:		System/Libraries
Provides:	libsmbclient

%description -n %{libname}
This package contains the SMB client library, part of the samba
suite of networking software, allowing other software to access
SMB shares.
%endif
%if %have_pversion && %build_system
%message_bugzilla %{libname}
%endif

%if %build_system
%package -n %{libname}-devel
URL:		http://www.samba.org
Summary: 	SMB Client Library Development files
Group:		Development/C
Provides:	libsmbclient-devel
Requires:       %{libname} = %{version}-%{release}

%description -n %{libname}-devel
This package contains the development files for the SMB client
library, part of the samba suite of networking software, allowing
the development of other software to access SMB shares.
%endif
%if %have_pversion && %build_system
%message_bugzilla %{libname}-devel
%endif

%if %build_system
%package -n %{libname}-static-devel
URL:            http://www.samba.org
Summary:        SMB Client Static Library Development files
Group:          System/Libraries
Provides:       libsmbclient-static-devel = %{version}-%{release}
Requires:       %{libname}-devel = %{version}-%{release}

%description -n %{libname}-static-devel
This package contains the static development files for the SMB
client library, part of the samba suite of networking software,
allowing the development of other software to access SMB shares.
%endif
%if %have_pversion && %build_system
%message_bugzilla %{libname}-devel
%endif

#%package passdb-ldap
#URL:		http://www.samba.org
#Summary:	Samba password database plugin for LDAP
#Group:		System/Libraries
#
#%description passdb-ldap
#The passdb-ldap package for samba provides a password database
#backend allowing samba to store account details in an LDAP
#database
#_if %have_pversion
#_message_bugzilla samba3-passdb-ldap
#_endif
#_if !%build_system
#_message_system
#_endif

%ifnarch alpha
%package passdb-mysql
URL:		http://www.samba.org
Summary:	Samba password database plugin for MySQL
Group:		System/Libraries
Requires:	%{name}-server = %{version}-%{release}
%endif
%ifnarch alpha && %build_system
Obsoletes:	samba3-passdb-mysql 
Provides:	samba3-passdb-mysql 
%endif
%ifnarch alpha

%description passdb-mysql
The passdb-mysql package for samba provides a password database
backend allowing samba to store account details in a MySQL
database
%endif

#does postgresql build on alpha?
#ifnarch alpha
%package passdb-pgsql
URL:		http://www.samba.org
Summary:	Samba password database plugin for PostgreSQL
Group:		System/Libraries
Requires:	%{name}-server = %{version}-%{release}
#endif
#ifnarch alpha && %build_system
%if %build_system
Obsoletes:	samba3-passdb-pgsql
Provides:	samba3-passdb-pgsql
%endif
#ifnarch alpha

%description passdb-pgsql
The passdb-pgsql package for samba provides a password database
backend allowing samba to store account details in a PostgreSQL
database
#endif

%package passdb-xml
URL:		http://www.samba.org
Summary:	Samba password database plugin for XML files
Group:		System/Libraries
Requires:	%{name}-server = %{version}-%{release}
%if %build_system
Obsoletes:	samba3-passdb-xml 
Provides:	samba3-passdb-xml 
%endif

%description passdb-xml
The passdb-xml package for samba provides a password database
backend allowing samba to store account details in XML files.
%if %have_pversion
%message_bugzilla samba3-passdb-xml
%endif
%if !%build_system
%message_system
%endif

#Antivirus packages:
%if %build_clamav
%package vscan-clamav
Summary: On-access virus scanning for samba using Clam Antivirus
Group: System/Servers
Requires: %{name}-server = %{version}
Provides: %{name}-vscan
Requires: clamd
%description vscan-clamav
A vfs-module for samba to implement on-access scanning using the
Clam antivirus scanner daemon.
%endif

%if %build_fprot
%package vscan-fprot
Summary: On-access virus scanning for samba using FPROT
Group: System/Servers
Requires: %{name}-server = %{version}
Provides: %{name}-vscan
%description vscan-fprot
A vfs-module for samba to implement on-access scanning using the
FPROT antivirus software (which must be installed to use this).
%endif

%if %build_fsav
%package vscan-fsecure
Summary: On-access virus scanning for samba using F-Secure
Group: System/Servers
Requires: %{name}-server = %{version}
Provides: %{name}-vscan
%description vscan-fsecure
A vfs-module for samba to implement on-access scanning using the
F-Secure antivirus software (which must be installed to use this).
%endif

%if %build_icap
%package vscan-icap
Summary: On-access virus scanning for samba using Clam Antivirus
Group: System/Servers
Requires: %{name}-server = %{version}
Provides: %{name}-icap
%description vscan-icap
A vfs-module for samba to implement on-access scanning using
ICAP-capable antivirus software.
%endif

%if %build_kaspersky
%package vscan-kaspersky
Summary: On-access virus scanning for samba using Kaspersky
Group: System/Servers
Requires: %{name}-server = %{version}
Provides: %{name}-vscan
%description vscan-kaspersky
A vfs-module for samba to implement on-access scanning using the
Kaspersky antivirus software (which must be installed to use this).
%endif

%if %build_mks
%package vscan-mks
Summary: On-access virus scanning for samba using MKS
Group: System/Servers
Requires: %{name}-server = %{version}
Provides: %{name}-vscan
%description vscan-mks
A vfs-module for samba to implement on-access scanning using the
MKS antivirus software (which must be installed to use this).
%endif

%if %build_nai
%package vscan-nai
Summary: On-access virus scanning for samba using NAI McAfee
Group: System/Servers
Requires: %{name}-server = %{version}
Provides: %{name}-vscan
%description vscan-nai
A vfs-module for samba to implement on-access scanning using the
NAI McAfee antivirus software (which must be installed to use this).
%endif

%if %build_openav
%package vscan-openav
Summary: On-access virus scanning for samba using OpenAntivirus
Group: System/Servers
Requires: %{name}-server = %{version}
Provides: %{name}-vscan
%description vscan-openav
A vfs-module for samba to implement on-access scanning using the
OpenAntivirus antivirus software (which must be installed to use this).
%endif

%if %build_sophos
%package vscan-sophos
Summary: On-access virus scanning for samba using Sophos
Group: System/Servers
Requires: %{name}-server = %{version}
Provides: %{name}-vscan
%description vscan-sophos
A vfs-module for samba to implement on-access scanning using the
Sophos antivirus software (which must be installed to use this).
%endif

%if %build_symantec
%package vscan-symantec
Summary: On-access virus scanning for samba using Symantec
Group: System/Servers
Requires: %{name}-server = %{version}
Provides: %{name}-vscan
Autoreq: 0
%description vscan-symantec
A vfs-module for samba to implement on-access scanning using the
Symantec antivirus software (which must be installed to use this).
%endif


%if %build_trend
%package vscan-trend
Summary: On-access virus scanning for samba using Trend
Group: System/Servers
Requires: %{name}-server = %{version}
Provides: %{name}-vscan
%description vscan-trend
A vfs-module for samba to implement on-access scanning using the
Trend antivirus software (which must be installed to use this).
%endif

%prep

# Allow users to query build options with --with options:
#%define opt_status(%1)	%(echo %{1})
%if %{?_with_options:1}%{!?_with_options:0}
%define opt_status(%{1})	%(if [ %{1} -eq 1 ];then echo enabled;else echo disabled;fi)
#exit 1
%{error: }
%{error:Build options available are:}
%{error:--with[out] system   Build as the system samba package [or as samba3]}
%{error:--with[out] acl      Build with support for file ACLs          - %opt_status %build_acl}
%{error:--with[out] winbind  Build with Winbind support                - %opt_status %build_winbind}
%{error:--with[out] wins     Build with WINS name resolution support   - %opt_status %build_wins}
%{error:--with[out] ldap     Build with legacy (samba2) LDAP support   - %opt_status %build_ldap}
%{error:--with[out] ads      Build with Active Directory support       - %opt_status %build_ads}
%{error:--with[out] scanners Enable on-access virus scanners           - %opt_status %build_scanners}
%{error: }
%else
%{error: }
%{error: This rpm has build options available, use --with options to see them}
%{error: }
%endif

%if %{?_with_options:1}%{!?_with_options:0} && %build_scanners
#{error:--with scanners enables the following:%{?build_clamav:clamav,}%{?build_icap:icap,}%{?build_fprot:fprot,}%{?build_mks:mks,}%{?build_openav:openav,}%{?build_sophos:sophos,}%{?build_symantec:symantec,}%{?build_trend:trend}}
%{error:--with scanners enables the following: clamav,icap,fprot,fsav,mks,nai,openav,sophos,trend}
%{error: }
%{error:To enable others (requires development libraries for the scanner):}
%{error:--with symantec           Enable on-access scanning with Symantec        - %opt_status %build_symantec}
%{error: }
%endif

%if %{?_with_options:1}%{!?_with_options:0}
clear
exit 1
%endif


%if %build_non_default
RPM_EXTRA_OPTIONS="\
%{?_with_system: --with system}\
%{?_without_system: --without system}\
%{?_with_acl: --with acl}\
%{?_without_acl: --without acl}\
%{?_with_winbind: --with winbind}\
%{?_without_winbind: --without winbind}\
%{?_with_wins: --with wins}\
%{?_without_wins: --without wins}\
%{?_with_ldap: --with ldap}\
%{?_without_ldap: --without ldap}\
%{?_with_ads: --with ads}\
%{?_without_ads: --without ads}\
%{?_with_scanners: --with scanners}\
%{?_without_scanners: --without scanners}\
"
echo "Building a non-default rpm with the following command-line arguments:"
echo "$RPM_EXTRA_OPTIONS"
echo "This rpm was built with non-default options, thus, to build ">%{SOURCE7}
echo "an identical rpm, you need to supply the following options">>%{SOURCE7}
echo "at build time: $RPM_EXTRA_OPTIONS">>%{SOURCE7}
echo -e "\n%{name}-%{version}-%{release}\n">>%{SOURCE7}
%else
echo "This rpm was built with default options">%{SOURCE7}
echo -e "\n%{name}-%{version}-%{release}\n">>%{SOURCE7}
%endif

%if %build_vscan
%setup -q -a 8 -n %{pkg_name}-%{source_ver}
%else
%setup -q -n %{pkg_name}-%{source_ver}
%endif
#%patch111 -p1
%patch1 -p1 -b .smbw
%patch4 -p1 -b .sbin
%patch5 -p1
# Version specific patches: current version
%if !%have_pversion
echo "Applying patches for current version: %{ver}"
#%patch7 -p1 -b .lib64
%patch9 -p1 -b .unixext
#%patch10 -p1 -b .rpcclient-libs
%patch11 -p1 -b .mdk
%else
# Version specific patches: upcoming version
echo "Applying patches for new versions: %{pversion}"
%patch8 -p1 -b .libsmbdir
%endif

# Limbo patches
%if %have_pversion && %have_pre
echo "Appling patches which should only be applied to prereleases"
%endif

# Fix quota compilation in glibc>2.3
%if %mdkversion >= 910 && %mdkversion < 1000
#grep "<linux/quota.h>" source/smbd/quotas.c >/dev/null && \
perl -pi -e 's@<linux/quota.h>@<sys/quota.h>@' source/smbd/quotas.c
%endif

cp %{SOURCE7} .

# Make a copy of examples so that we have a clean one for doc:
cp -a examples examples.bin

%if %build_vscan
cp -a %{vscandir} %{vfsdir}
#fix stupid directory names:
#mv %{vfsdir}/%{vscandir}/openantivirus %{vfsdir}/%{vscandir}/oav
# Inline replacement of config dir
for av in clamav fprotd fsav icap kavp mksd mcdaemon oav sophos symantec trend
 do
	[ -e %{vfsdir}/%{vscandir}/*/vscan-$av.h ] && perl -pi -e \
	's,^#define PARAMCONF "/etc/samba,#define PARAMCONF "/etc/%{name},' \
	%{vfsdir}/%{vscandir}/*/vscan-$av.h
done
#Inline edit vscan header:
perl -pi -e 's/^# define SAMBA_VERSION_MAJOR 2/# define SAMBA_VERSION_MAJOR 3/g;s/# define SAMBA_VERSION_MINOR 2/# define SAMBA_VERSION_MINOR 0/g' %{vfsdir}/%{vscandir}/include/vscan-global.h
%endif

# Edit some files when not building system samba:
%if !%build_system
perl -pi -e 's/%{pkg_name}/%{name}/g' source/auth/pampass.c
%endif

#remove cvs internal files from docs:
find docs examples -name '.cvsignore' -exec rm -f {} \;

#make better doc trees:
chmod -R a+rX examples docs *Manifest* README  Roadmap COPYING
mkdir -p clean-docs/samba-doc
cp -a examples docs clean-docs/samba-doc
mv -f clean-docs/samba-doc/examples/libsmbclient clean-docs/
rm -Rf clean-docs/samba-doc/docs/{docbook,manpages,htmldocs,using_samba}
ln -s %{_datadir}/swat%{samba_major}/using_samba/ clean-docs/samba-doc/docs/using_samba
ln -sf %{_datadir}/swat%{samba_major}/help/ clean-docs/samba-doc/docs/htmldocs

%build
#%serverbuild
(cd source
CFLAGS=`echo "$RPM_OPT_FLAGS"|sed -e 's/-g//g'`
%if %gcc331
CFLAGS=`echo "$CFLAGS"|sed -e 's/-O2/-O/g'`
%endif
./autogen.sh
# Don't use --with-fhs now, since it overrides libdir, it sets configdir, 
# lockdir,piddir logfilebase,privatedir and swatdir
%configure      --prefix=%{_prefix} \
                --sysconfdir=%{_sysconfdir}/%{name} \
                --localstatedir=/var \
                --with-libdir=%{_libdir}/%{name} \
                --with-privatedir=%{_sysconfdir}/%{name} \
		--with-lockdir=/var/cache/%{name} \
		--with-piddir=/var/run \
                --with-swatdir=%{_datadir}/swat%{samba_major} \
                --with-configdir=%{_sysconfdir}/%{name} \
		--with-logfilebase=/var/log/%{name} \
%if !%build_ads
		--with-ads=no	\
%endif
                --with-automount \
                --with-smbmount \
                --with-pam \
                --with-pam_smbpass \
%if %build_ldap
		--with-ldapsam \
%endif
		--with-tdbsam \
                --with-syslog \
                --with-quotas \
                --with-utmp \
		--with-manpages-langs=en \
%if %build_acl
		--with-acl-support      \
%endif
		--disable-mysqltest \
		--with-expsam=%build_expsam \
		--program-suffix=%{samba_major} 
#		--with-shared-modules=pdb_ldap,idmap_ldap \
#		--with-manpages-langs=en,ja,pl	\
#_if !%build_system
#                --with-smbwrapper \
#_endif		
#		--with-nisplussam \
#                --with-fhs \

#Fix the make file so we don't create debug information on 9.2
%if %mdkversion == 920
perl -pi -e 's/-g //g' Makefile
%endif

perl -pi -e 's|-Wl,-rpath,%{_libdir}||g;s|-Wl,-rpath -Wl,%{_libdir}||g' Makefile

make proto_exists
%make all libsmbclient smbfilter wins modules %{?_with_test: torture debug3html bin/log2pcap} bin/editreg bin/smbget client/mount.cifs

)

# Build mkntpasswd in examples/LDAP/ for smbldaptools
make -C examples.bin/LDAP/smbldap-tools/mkntpwd

%if %build_vscan
echo -e "\n\nBuild antivirus VFS modules\n\n"
pushd %{vfsdir}/%{vscandir}
%configure
#sed -i -e 's,openantivirus,oav,g' Makefile
sed -i -e 's,^\(.*clamd socket name.*=\).*,\1 /var/lib/clamav/clamd.socket,g' clamav/vscan-clamav.conf
make
popd
%endif

# Build antivirus vfs objects
%if %build_symantec
echo "Building Symantec"
make -C %{vfsdir}/%{vscandir} symantec
%endif

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT

#Ensure all docs are readable
chmod a+r docs -R

# Any entries here mean samba makefile is *really* broken:
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/%{name}
mkdir -p $RPM_BUILD_ROOT/%{_datadir}
mkdir -p $RPM_BUILD_ROOT%{_libdir}/%{name}/vfs

(cd source
make DESTDIR=$RPM_BUILD_ROOT LIBDIR=%{_libdir}/%{name} MANDIR=%{_mandir} install installclientlib installmodules)

install -m755 source/bin/{editreg,smbget} %{buildroot}/%{_bindir}

#need to stay
mkdir -p $RPM_BUILD_ROOT/{sbin,bin}
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/{logrotate.d,pam.d,xinetd.d}
mkdir -p $RPM_BUILD_ROOT/%{_initrddir}
mkdir -p $RPM_BUILD_ROOT/var/cache/%{name}
mkdir -p $RPM_BUILD_ROOT/var/log/%{name}
mkdir -p $RPM_BUILD_ROOT/var/run/%{name}
mkdir -p $RPM_BUILD_ROOT/var/spool/%{name}
mkdir -p $RPM_BUILD_ROOT/%{_localstatedir}/%{name}/{netlogon,profiles,printers}
mkdir -p $RPM_BUILD_ROOT/%{_localstatedir}/%{name}/printers/{W32X86,WIN40,W32ALPHA,W32MIPS,W32PPC}
mkdir -p $RPM_BUILD_ROOT/%{_localstatedir}/%{name}/codepages/src
mkdir -p $RPM_BUILD_ROOT/%{_lib}/security
mkdir -p $RPM_BUILD_ROOT%{_libdir}
mkdir -p $RPM_BUILD_ROOT%{_libdir}/%{name}/vfs
mkdir -p $RPM_BUILD_ROOT%{_datadir}/%{name}/scripts

#smbwrapper and pam_winbind not handled by make, pam_smbpass.so doesn't build
#install -m 755 source/bin/smbwrapper.so $RPM_BUILD_ROOT%{_libdir}/smbwrapper%{samba_major}.so
install -m 755 source/bin/pam_smbpass.so $RPM_BUILD_ROOT/%{_lib}/security/pam_smbpass%{samba_major}.so
install -m 755 source/nsswitch/pam_winbind.so $RPM_BUILD_ROOT/%{_lib}/security/pam_winbind.so

install -m755 source/bin/libsmbclient.a $RPM_BUILD_ROOT%{_libdir}/libsmbclient.a
pushd $RPM_BUILD_ROOT/%{_libdir}
[ -f libsmbclient.so ] && mv -f libsmbclient.so libsmbclient.so.%{libsmbmajor}
ln -sf libsmbclient.so.%{libsmbmajor} libsmbclient.so
popd

# smbsh forgotten
#install -m 755 source/bin/smbsh $RPM_BUILD_ROOT%{_bindir}/

%if %build_vscan
%makeinstall_std -C %{vfsdir}/%{vscandir}
install -m 644 %{vfsdir}/%{vscandir}/*/vscan-*.conf %{buildroot}/%{_sysconfdir}/%{name}
%endif
	
#libnss_* not handled by make:
# Install the nsswitch library extension file
for i in wins winbind; do
  install -m755 source/nsswitch/libnss_${i}.so $RPM_BUILD_ROOT/%{_lib}/libnss_${i}.so
done
# Make link for wins and winbind resolvers
( cd $RPM_BUILD_ROOT/%{_lib}; ln -s libnss_wins.so libnss_wins.so.2; ln -s libnss_winbind.so libnss_winbind.so.2)

%if %{?_with_test:1}%{!?_with_test:0}
for i in {%{testbin}};do
  install -m755 source/bin/${i} $RPM_BUILD_ROOT/%{_bindir}/${i}%{samba_major}
done
%endif

# Install other stuff

#        install -m644 examples/VFS/recycle/recycle.conf $RPM_BUILD_ROOT%{_sysconfdir}/samba/
        install -m644 packaging/Mandrake/smbusers $RPM_BUILD_ROOT%{_sysconfdir}/%{name}/smbusers
        install -m755 packaging/Mandrake/smbprint $RPM_BUILD_ROOT/%{_bindir}
        #install -m755 packaging/RedHat/smbadduser $RPM_BUILD_ROOT/usr/bin
        install -m755 packaging/Mandrake/findsmb $RPM_BUILD_ROOT/%{_bindir}
        install -m755 packaging/Mandrake/smb.init $RPM_BUILD_ROOT/%{_initrddir}/smb%{samba_major}
        install -m755 packaging/Mandrake/smb.init $RPM_BUILD_ROOT/%{_sbindir}/%{name}
	install -m755 packaging/Mandrake/winbind.init $RPM_BUILD_ROOT/%{_initrddir}/winbind
#	install -m755 packaging/Mandrake/wrepld.init $RPM_BUILD_ROOT/%{_initrddir}/wrepld%{samba_major}
	install -m755 packaging/Mandrake/winbind.init $RPM_BUILD_ROOT/%{_sbindir}/winbind
        install -m644 packaging/Mandrake/samba.pamd $RPM_BUILD_ROOT/%{_sysconfdir}/pam.d/%{name}
	install -m644 packaging/Mandrake/system-auth-winbind.pamd $RPM_BUILD_ROOT/%{_sysconfdir}/pam.d/system-auth-winbind
#
        install -m644 %{SOURCE1} $RPM_BUILD_ROOT/%{_sysconfdir}/logrotate.d/%{name}
#	install -m644 packaging/Mandrake/samba-slapd-include.conf $RPM_BUILD_ROOT%{_sysconfdir}/%{name}/samba-slapd.include

# Install smbldap-tools scripts:
for i in examples/LDAP/smbldap-tools/*.pl; do
	install -m 750 $i $RPM_BUILD_ROOT/%{_datadir}/%{name}/scripts/
	ln -s %{_datadir}/%{name}/scripts/`basename $i` $RPM_BUILD_ROOT/%{_bindir}/`basename $i|sed -e 's/\.pl//g'`%{samba_major}
done

install -m 750 examples/LDAP/smbldap-tools/smbldap_tools.pm $RPM_BUILD_ROOT/%{_datadir}/%{name}/scripts/

# The conf file	
install -m 640 examples/LDAP/smbldap-tools/smbldap_conf.pm $RPM_BUILD_ROOT/%{_sysconfdir}/%{name}

#Fix the smbldap-tools when not system samba:
%if !%build_system
perl -pi -e 's/^(use|package)(\s+)smbldap_(\w+);$/${1}${2}smbldap_${3}%{samba_major};/g' \
%{buildroot}/%{_sysconfdir}/%{name}/smbldap_conf.pm \
%{buildroot}/%{_datadir}/%{name}/scripts/smbldap*.p?
perl -pi -e 's,/usr/local/sbin/mkntpwd,/usr/sbin/mkntpwd%{samba_major},g;s,553,421,g' %{buildroot}/%{_sysconfdir}/%{name}/smbldap_conf.pm
perl -pi -e 's,\$smbldap_conf::SID,\$smbldap_conf3::SID,g' %{buildroot}/%{_datadir}/%{name}/scripts/smbldap*.p?
%endif
perl -pi -e 's,/usr/local/sbin/smbldap-passwd.pl,%{_datadir}/%{name}/scripts/smbldap-passwd.pl,g' %{buildroot}/%{_datadir}/%{name}/scripts/smbldap-useradd.pl 

# Link both smbldap*.pm into vendor-perl (any better ideas?)
mkdir -p %{buildroot}/%{perl_vendorlib}
ln -s %{_sysconfdir}/%{name}/smbldap_conf.pm $RPM_BUILD_ROOT/%{perl_vendorlib}/smbldap_conf%{samba_major}.pm
ln -s %{_datadir}/%{name}/scripts/smbldap_tools.pm $RPM_BUILD_ROOT/%{perl_vendorlib}/smbldap_tools%{samba_major}.pm
#mkntpwd
install -m750 examples.bin/LDAP/smbldap-tools/mkntpwd/mkntpwd %{buildroot}/%{_sbindir}/mkntpwd%{samba_major}

# Samba smbpasswd migration script:
install -m755 examples/LDAP/convertSambaAccount $RPM_BUILD_ROOT/%{_datadir}/%{name}/scripts/

# make a conf file for winbind from the default one:
	cat packaging/Mandrake/smb.conf|sed -e  's/^;  winbind/  winbind/g;s/^;  obey pam/  obey pam/g; s/^;   printer admin = @"D/   printer admin = @"D/g;s/^;   password server = \*/   password server = \*/g;s/^;  template/  template/g; s/^   security = user/   security = domain/g' > packaging/Mandrake/smb-winbind.conf
        install -m644 packaging/Mandrake/smb-winbind.conf $RPM_BUILD_ROOT/%{_sysconfdir}/%{name}/smb-winbind.conf

# Some inline fixes for smb.conf for non-winbind use
install -m644 packaging/Mandrake/smb.conf $RPM_BUILD_ROOT/%{_sysconfdir}/%{name}/smb.conf
cat packaging/Mandrake/smb.conf | \
sed -e 's/^;   printer admin = @adm/   printer admin = @adm/g' >$RPM_BUILD_ROOT/%{_sysconfdir}/%{name}/smb.conf
%if %build_cupspc
perl -pi -e 's/printcap name = lpstat/printcap name = cups/g' $RPM_BUILD_ROOT/%{_sysconfdir}/%{name}/smb.conf
perl -pi -e 's/printcap name = lpstat/printcap name = cups/g' $RPM_BUILD_ROOT/%{_sysconfdir}/%{name}/smb-winbind.conf
%endif

#%if !%build_system
# Fix script paths in smb.conf
#perl -pi -e 's,%{_datadir}/samba,%{_datadir}/%{name},g' %{buildroot}/%{_sysconfdir}/%{name}/smb*.conf
#%endif


#install mount.cifs
install -m755 source/client/mount.cifs %{buildroot}/bin/mount.cifs%{samba_major}

        echo 127.0.0.1 localhost > $RPM_BUILD_ROOT/%{_sysconfdir}/%{name}/lmhosts

# Link smbspool to CUPS (does not require installed CUPS)

        mkdir -p $RPM_BUILD_ROOT/%{_libdir}/cups/backend
        ln -s %{_bindir}/smbspool%{alternative_major} $RPM_BUILD_ROOT/%{_libdir}/cups/backend/smb%{alternative_major}

# xinetd support

        mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}/xinetd.d
        install -m644 %{SOURCE3} $RPM_BUILD_ROOT/%{_sysconfdir}/xinetd.d/swat%{samba_major}

# menu support

mkdir -p $RPM_BUILD_ROOT%{_menudir}
cat > $RPM_BUILD_ROOT%{_menudir}/%{name}-swat << EOF
?package(%{name}-swat):\
command="gnome-moz-remote http://localhost:901/" \
needs="gnome" \
icon="swat%{samba_major}.png" \
section="Configuration/Networking" \
title="Samba Configuration (SWAT)" \
longtitle="The Swat Samba Administration Tool"
?package(%{name}-swat):\
command="sh -c '\$BROWSER http://localhost:901/'" \ 
needs="x11" \
icon="swat%{samba_major}.png" \
section="Configuration/Networking" \
title="Samba Configuration (SWAT)" \
longtitle="The Swat Samba Administration Tool"
EOF

mkdir -p $RPM_BUILD_ROOT%{_liconsdir} $RPM_BUILD_ROOT%{_iconsdir} $RPM_BUILD_ROOT%{_miconsdir}

bzcat %{SOURCE4} > $RPM_BUILD_ROOT%{_liconsdir}/swat%{samba_major}.png
bzcat %{SOURCE5} > $RPM_BUILD_ROOT%{_iconsdir}/swat%{samba_major}.png
bzcat %{SOURCE6} > $RPM_BUILD_ROOT%{_miconsdir}/swat%{samba_major}.png

bzcat %{SOURCE10}> $RPM_BUILD_ROOT%{_datadir}/%{name}/scripts/print-pdf
bzcat %{SOURCE11}> $RPM_BUILD_ROOT%{_datadir}/%{name}/scripts/smb-migrate

# Fix configs when not building system samba:

#Client binaries will have suffixes while we use alternatives, even
# if we are system samba
%if !%build_system || %build_alternatives
for OLD in %{buildroot}/%{_bindir}/{%{clientbin}} %{buildroot}/bin/%{client_bin} %{buildroot}/%{_libdir}/cups/backend/smb
do
    NEW=`echo ${OLD}%{alternative_major}`
    [ -e $OLD ] && mv -f $OLD $NEW
done
for OLD in %{buildroot}/%{_mandir}/man?/{%{clientbin}}* %{buildroot}/%{_mandir}/man?/%{client_bin}*
do
    if [ -e $OLD ]
    then
        BASE=`perl -e '$_="'${OLD}'"; m,(%buildroot)(.*?)(\.[0-9]),;print "$1$2\n";'`
        EXT=`echo $OLD|sed -e 's,'${BASE}',,g'`
        NEW=`echo ${BASE}%{alternative_major}${EXT}`
        mv $OLD $NEW
    fi
done		
%endif
rm -f %{buildroot}/sbin/mount.smbfs
# Link smbmount to /sbin/mount.smb and /sbin/mount.smbfs
#I don't think it's possible for make to do this ...
(cd $RPM_BUILD_ROOT/sbin
        ln -s ..%{_bindir}/smbmount%{alternative_major} mount.smb%{alternative_major}
        ln -s ..%{_bindir}/smbmount%{alternative_major} mount.smbfs%{alternative_major}
)
# Server/common binaries are versioned only if not system samba:
%if !%build_system
for OLD in %{buildroot}/%{_bindir}/{%{commonbin}} %{buildroot}/%{_bindir}/{%{serverbin}} %{buildroot}/%{_sbindir}/{%{serversbin},swat}
do
    NEW=`echo ${OLD}%{alternative_major}`
    mv $OLD $NEW -f ||:
done
# And the man pages too:
for OLD in %{buildroot}/%{_mandir}/man?/{%{commonbin},%{serverbin},%{serversbin},swat,{%testbin},smb.conf,lmhosts}*
do
    if [ -e $OLD ]
    then
        BASE=`perl -e '$_="'${OLD}'"; m,(%buildroot)(.*?)(\.[0-9]),;print "$1$2\n";'`
#        BASE=`perl -e '$name="'${OLD}'"; print "",($name =~ /(.*?)\.[0-9]/), "\n";'`
	EXT=`echo $OLD|sed -e 's,'${BASE}',,g'`
	NEW=`echo ${BASE}%{samba_major}${EXT}`
	mv $OLD $NEW
    fi
done		
# Replace paths in config files and init scripts:
for i in smb ;do
	perl -pi -e 's,/subsys/'$i',/subsys/'$i'%{samba_major},g' $RPM_BUILD_ROOT/%{_initrddir}/${i}%{samba_major}
done
for i in %{_sysconfdir}/%{name}/smb.conf %{_initrddir}/smb%{samba_major} %{_sbindir}/%{name} %{_initrddir}/winbind /%{_sysconfdir}/logrotate.d/%{name} /%{_sysconfdir}/xinetd.d/swat%{samba_major} %{_initrddir}/wrepld%{samba_major}; do
	perl -pi -e 's,/%{pkg_name},/%{name},g; s,smbd,%{_sbindir}/smbd%{samba_major},g; s,nmbd,%{_sbindir}/nmbd%{samba_major},g; s,/usr/sbin/swat,%{_sbindir}/swat%{samba_major},g;s,wrepld,%{_sbindir}/wrepld%{samba_major},g' $RPM_BUILD_ROOT/$i;
done
# Fix xinetd file for swat:
perl -pi -e 's,/usr/sbin,%{_sbindir},g' $RPM_BUILD_ROOT/%{_sysconfdir}/xinetd.d/swat%{samba_major}
%endif

#Clean up unpackaged files:
for i in %{_bindir}/pam_smbpass.so %{_bindir}/smbwrapper.so;do
rm -f %{buildroot}/$i
done

# (sb) make a smb.conf.clean we can use for the merge, since an existing
# smb.conf won't get overwritten
cp $RPM_BUILD_ROOT/%{_sysconfdir}/%{name}/smb.conf $RPM_BUILD_ROOT/%{_datadir}/%{name}/smb.conf.clean

# (sb) leave a README.mdk.conf to explain what has been done
cat << EOF > $RPM_BUILD_ROOT/%{_datadir}/%{name}/README.mdk.conf
In order to facilitate upgrading an existing samba install, and merging
previous configuration data with any new syntax used by samba3, a merge
script has attempted to combine your local configuration data with the
new conf file format.  The merged data is in smb.conf, with comments like

	# *** merged from original smb.conf: ***

near the additional entries.  Any local shares should have been appended to
smb.conf.  A log of what took place should be in: 

	/var/log/samba/smb-migrate.log

A clean samba3 smb.conf is in /usr/share/samba, named smb.conf.clean.
Your original conf should be /etc/samba/smb.conf.tomerge.

The actual merge script is /usr/share/samba/scripts/smb-migrate.

Questions/issues: sbenedict@mandrakesoft.com

EOF

%clean
rm -rf $RPM_BUILD_ROOT

%post server

%_post_service smb%{samba_major}
#%_post_service wrepld%{samba_major}

# Add a unix group for samba machine accounts
groupadd -frg 421 machines

# Migrate tdb's from /var/lock/samba (taken from official samba spec file):
for i in /var/lock/samba/*.tdb
do
if [ -f $i ]; then
	newname=`echo $i | sed -e's|var\/lock\/samba|var\/cache\/samba|'`
	echo "Moving $i to $newname"
	mv $i $newname
fi
done

%post common
# Basic migration script for pre-2.2.1 users,
# since smb config moved from /etc to %{_sysconfdir}/samba

# Let's create a proper %{_sysconfdir}/samba/smbpasswd file
[ -f %{_sysconfdir}/%{name}/smbpasswd ] || {
	echo "Creating password file for samba..."
	touch %{_sysconfdir}/%{name}/smbpasswd
}

# And this too, in case we don't have smbd to create it for us
[ -f /var/cache/%{name}/unexpected.tdb ] || {
	touch /var/cache/%{name}/unexpected.tdb
}

# Let's define the proper paths for config files
perl -pi -e 's/(\/etc\/)(smb)/\1%{name}\/\2/' %{_sysconfdir}/%{name}/smb.conf

# Fix the logrotate.d file from smb and nmb to smbd and nmbd
if [ -f %{_sysconfdir}/logrotate.d/samba ]; then
        perl -pi -e 's/smb /smbd /' %{_sysconfdir}/logrotate.d/samba
        perl -pi -e 's/nmb /nmbd /' %{_sysconfdir}/logrotate.d/samba
fi

# And not loose our machine account SID
[ -f %{_sysconfdir}/MACHINE.SID ] && mv -f %{_sysconfdir}/MACHINE.SID %{_sysconfdir}/%{name}/ ||:

%triggerpostun common -- samba-common < 3.0.1-3mdk
# (sb) merge any existing smb.conf with new syntax file
if [ $1 = 2 ]; then
	# (sb) save existing smb.conf for merge
	echo "Upgrade: copy smb.conf to smb.conf.tomerge for merging..."
	cp -f %{_sysconfdir}/%{name}/smb.conf %{_sysconfdir}/%{name}/smb.conf.tomerge
	echo "Upgrade: merging previous smb.conf..."
	if [ -f %{_datadir}/%{name}/smb.conf.clean ]; then
		cp %{_datadir}/%{name}/smb.conf.clean %{_sysconfdir}/%{name}/smb.conf
		cp %{_datadir}/%{name}/README.mdk.conf %{_sysconfdir}/%{name}/
		%{_datadir}/%{name}/scripts/smb-migrate commit
	fi
fi

%postun common
if [ -f %{_sysconfdir}/%{name}/README.mdk.conf ];then rm -f %{_sysconfdir}/%{name}/README.mdk.conf;fi

%if %build_winbind
%post winbind
if [ $1 = 1 ]; then
    /sbin/chkconfig winbind on
    cp -af %{_sysconfdir}/nsswitch.conf %{_sysconfdir}/nsswitch.conf.rpmsave
    cp -af %{_sysconfdir}/nsswitch.conf %{_sysconfdir}/nsswitch.conf.rpmtemp
    for i in passwd group;do
        grep ^$i %{_sysconfdir}/nsswitch.conf |grep -v 'winbind' >/dev/null
        if [ $? = 0 ];then
            echo "Adding a winbind entry to the $i section of %{_sysconfdir}/nsswitch.conf"
            awk '/^'$i'/ {print $0 " winbind"};!/^'$i'/ {print}' %{_sysconfdir}/nsswitch.conf.rpmtemp >%{_sysconfdir}/nsswitch.conf;
	    cp -af %{_sysconfdir}/nsswitch.conf %{_sysconfdir}/nsswitch.conf.rpmtemp
        else
            echo "$i entry found in %{_sysconfdir}/nsswitch.conf"
        fi
    done
    if [ -f %{_sysconfdir}/nsswitch.conf.rpmtemp ];then rm -f %{_sysconfdir}/nsswitch.conf.rpmtemp;fi
fi

%preun winbind
if [ $1 = 0 ]; then
	echo "Removing winbind entries from %{_sysconfdir}/nsswitch.conf"
	perl -pi -e 's/ winbind//' %{_sysconfdir}/nsswitch.conf

	/sbin/chkconfig winbind reset
fi
%endif %build_winbind

%if %build_wins
%post -n nss_wins%{samba_major}
if [ $1 = 1 ]; then
    cp -af %{_sysconfdir}/nsswitch.conf %{_sysconfdir}/nsswitch.conf.rpmsave
    grep '^hosts' %{_sysconfdir}/nsswitch.conf |grep -v 'wins' >/dev/null
    if [ $? = 0 ];then
        echo "Adding a wins entry to the hosts section of %{_sysconfdir}/nsswitch.conf"
        awk '/^hosts/ {print $0 " wins"};!/^hosts/ {print}' %{_sysconfdir}/nsswitch.conf.rpmsave >%{_sysconfdir}/nsswitch.conf;
    else
        echo "wins entry found in %{_sysconfdir}/nsswitch.conf"
    fi
#    else
#        echo "Upgrade, leaving nsswitch.conf intact"
fi

%preun -n nss_wins%{samba_major}
if [ $1 = 0 ]; then
	echo "Removing wins entry from %{_sysconfdir}/nsswitch.conf"
	perl -pi -e 's/ wins//' %{_sysconfdir}/nsswitch.conf
#else
#	echo "Leaving %{_sysconfdir}/nsswitch.conf intact"
fi
%endif %build_wins

%preun server

%_preun_service smb%{samba_major}
#%_preun_service wrepld%{samba_major}

if [ $1 = 0 ] ; then
#    /sbin/chkconfig --level 35 smb reset
# Let's not loose /var/cache/samba

    if [ -d /var/cache/%{name} ]; then
      mv -f /var/cache/%{name} /var/cache/%{name}.BAK
    fi
fi

%post swat
if [ -f /var/lock/subsys/xinetd ]; then
        service xinetd reload >/dev/null 2>&1 || :
fi
%update_menus

%postun swat

# Remove swat entry from xinetd
if [ $1 = 0 -a -f %{_sysconfdir}/xinetd.conf ] ; then
rm -f %{_sysconfdir}/xinetd.d/swat%{samba_major}
	service xinetd reload &>/dev/null || :
fi

if [ "$1" = "0" -a -x /usr/bin/update-menus ]; then /usr/bin/update-menus || true ; fi

%clean_menus

%if %build_system
%post -n %{libname} -p /sbin/ldconfig
%postun -n %{libname} -p /sbin/ldconfig
%endif

%if %build_alternatives
%post client

update-alternatives --install %{_bindir}/smbclient smbclient \
%{_bindir}/smbclient%{alternative_major} 10 \
$(for i in {/bin/mount.cifs,/sbin/{%{client_sbin}},%{_bindir}/{%{clientbin}}};do
j=`basename $i`
[ "$j" = "smbclient" ] || \
echo -n " --slave ${i} ${j} ${i}%{alternative_major}";done) \
--slave %{_libdir}/cups/backend/smb cups_smb %{_libdir}/cups/backend/smb%{alternative_major} || \
update-alternatives --auto smbclient

%preun client
[ $1 = 0 ] && update-alternatives --remove smbclient %{_bindir}/smbclient%{alternative_major} ||:
%endif

%if %build_alternatives
%triggerpostun client -- samba-client, samba2-client
[ ! -e %{_bindir}/smbclient ] && update-alternatives --auto smbclient || :
%endif

%files server
%defattr(-,root,root)
%(for i in %{_sbindir}/{%{serversbin}}%{samba_major};do echo $i;done)
%(for i in %{_bindir}/{%{serverbin}}%{samba_major};do echo $i;done)
%attr(755,root,root) /%{_lib}/security/pam_smbpass*
%dir %{_libdir}/%{name}/vfs
%{_libdir}/%{name}/vfs/*.so
%if %build_vscan
%exclude %{_libdir}/%{name}/vfs/vscan*.so
%endif
%dir %{_libdir}/%{name}/pdb

%attr(-,root,root) %config(noreplace) %{_sysconfdir}/%{name}/smbusers
%attr(-,root,root) %config(noreplace) %{_initrddir}/smb%{samba_major}
#%attr(-,root,root) %config(noreplace) %{_initrddir}/wrepld%{samba_major}
%attr(-,root,root) %config(noreplace) %{_sysconfdir}/logrotate.d/%{name}
%attr(-,root,root) %config(noreplace) %{_sysconfdir}/pam.d/%{name}
#%attr(-,root,root) %config(noreplace) %{_sysconfdir}/%{name}/samba-slapd.include
%(for i in %{_mandir}/man?/{%{serverbin},%{serversbin}}%{samba_major}\.[0-9]*;do echo $i|grep -v mkntpwd;done)
%attr(775,root,adm) %dir %{_localstatedir}/%{name}/netlogon
%attr(755,root,root) %dir %{_localstatedir}/%{name}/profiles
%attr(755,root,root) %dir %{_localstatedir}/%{name}/printers
%attr(2775,root,adm) %dir %{_localstatedir}/%{name}/printers/*
%attr(1777,root,root) %dir /var/spool/%{name}
%dir %{_datadir}/%{name}
%dir %{_datadir}/%{name}/scripts
%attr(0755,root,root) %{_datadir}/%{name}/scripts/print-pdf
%attr(0750,root,adm) %{_datadir}/%{name}/scripts/smbldap*.pl
%attr(0750,root,adm) %{_bindir}/smbldap*
%attr(0640,root,adm) %config(noreplace) %{_sysconfdir}/%{name}/smbldap_conf.pm
%attr(0644,root,root) %{_datadir}/%{name}/scripts/smbldap_tools.pm
%{perl_vendorlib}/*.pm
#%attr(0700,root,root) %{_datadir}/%{name}/scripts/*port_smbpasswd.pl
%attr(0755,root,root) %{_datadir}/%{name}/scripts/convertSambaAccount


%files doc
%defattr(-,root,root)
%doc README COPYING Manifest Read-Manifest-Now
%doc WHATSNEW.txt Roadmap
%doc README.%{name}-mandrake-rpm
%doc clean-docs/samba-doc/docs
%doc clean-docs/samba-doc/examples
%attr(-,root,root) %{_datadir}/swat%{samba_major}/using_samba/

%files swat
%defattr(-,root,root)
%config(noreplace) %{_sysconfdir}/xinetd.d/swat%{samba_major}
#%attr(-,root,root) /sbin/*
%{_sbindir}/swat%{samba_major}
%{_menudir}/%{name}-swat
%{_miconsdir}/*.png
%{_liconsdir}/*.png
%{_iconsdir}/*.png
%attr(-,root,root) %{_datadir}/swat%{samba_major}/help/
%attr(-,root,root) %{_datadir}/swat%{samba_major}/images/
%attr(-,root,root) %{_datadir}/swat%{samba_major}/include/
%lang(ja) %{_datadir}/swat%{samba_major}/lang/ja
%lang(tr) %{_datadir}/swat%{samba_major}/lang/tr
%{_mandir}/man8/swat*.8*
%lang(de) %{_libdir}/%{name}/de.msg
%lang(en) %{_libdir}/%{name}/en.msg
%lang(fr) %{_libdir}/%{name}/fr.msg
%lang(it) %{_libdir}/%{name}/it.msg
%lang(ja) %{_libdir}/%{name}/ja.msg
%lang(nl) %{_libdir}/%{name}/nl.msg
%lang(pl) %{_libdir}/%{name}/pl.msg
%lang(tr) %{_libdir}/%{name}/tr.msg
#%doc swat/README

%files client
%defattr(-,root,root)
%(for i in %{_bindir}/{%{clientbin}}%{alternative_major};do echo $i;done)
%(for i in %{_mandir}/man?/{%{clientbin}}%{alternative_major}.?.*;do echo $i|grep -v smbprint;done)
#xclude %{_mandir}/man?/smbget*
%{_mandir}/man5/smbgetrc%{alternative_major}.5*
%ifnarch alpha
%(for i in /sbin/{%{client_sbin}}%{alternative_major};do echo $i;done)
%attr(4755,root,root) /bin/mount.cifs%{alternative_major}
%attr(755,root,root) %{_bindir}/smbmount%{alternative_major}
%attr(4755,root,root) %{_bindir}/smbumount%{alternative_major}
%attr(4755,root,root) %{_bindir}/smbmnt%{alternative_major}
%{_mandir}/man8/smbmnt*.8*
%{_mandir}/man8/smbmount*.8*
%{_mandir}/man8/smbumount*.8*
%{_mandir}/man8/mount.cifs*.8*
%else
%exclude %{_bindir}/smb*m*nt%{samba_major}
%exclude %{_mandir}/man8/smb*m*nt*.8*
%endif
# Link of smbspool to CUPS
/%{_libdir}/cups/backend/smb%{alternative_major}

%files common
%defattr(-,root,root)
%dir /var/cache/%{name}
%dir /var/log/%{name}
%dir /var/run/%{name}
%(for i in %{_bindir}/{%{commonbin},tdbtool}%{samba_major};do echo $i;done)
%(for i in %{_mandir}/man?/{%{commonbin}}%{samba_major}\.[0-9]*;do echo $i;done)
#%{_libdir}/smbwrapper%{samba_major}.so
%dir %{_libdir}/%{name}
%{_libdir}/%{name}/*.dat
%{_libdir}/%{name}/charset
#%{_libdir}/%{name}/lowcase.dat
#%{_libdir}/%{name}/valid.dat
%dir %{_sysconfdir}/%{name}
%attr(-,root,root) %config(noreplace) %{_sysconfdir}/%{name}/smb.conf
%attr(-,root,root) %config(noreplace) %{_sysconfdir}/%{name}/smb-winbind.conf
%attr(-,root,root) %config(noreplace) %{_sysconfdir}/%{name}/lmhosts
%dir %{_localstatedir}/%{name}
%attr(-,root,root) %{_localstatedir}/%{name}/codepages
%{_mandir}/man5/smb.conf*.5*
%{_mandir}/man5/lmhosts*.5*
#%{_mandir}/man7/Samba*.7*
%dir %{_datadir}/swat%{samba_major}
%attr(0750,root,adm) %{_datadir}/%{name}/scripts/smb-migrate
%attr(-,root,root) %{_datadir}/%{name}/smb.conf.clean
%attr(-,root,root) %{_datadir}/%{name}/README.mdk.conf

%if %build_winbind
%files winbind
%defattr(-,root,root)
%{_sbindir}/winbindd
%{_sbindir}/winbind
%{_bindir}/wbinfo
%attr(755,root,root) /%{_lib}/security/pam_winbind*
%attr(755,root,root) /%{_lib}/libnss_winbind*
%attr(-,root,root) %config(noreplace) %{_initrddir}/winbind
%attr(-,root,root) %config(noreplace) %{_sysconfdir}/pam.d/system-auth-winbind*
%{_mandir}/man8/winbindd*.8*
%{_mandir}/man1/wbinfo*.1*
%endif

%if %build_wins
%files -n nss_wins%{samba_major}
%defattr(-,root,root)
%attr(755,root,root) /%{_lib}/libnss_wins.so*
%endif

%if %{?_with_test:1}%{!?_with_test:0}
%files test
%defattr(-,root,root)
%(for i in %{_bindir}/{%{testbin}}%{samba_major};do echo $i;done)
%{_mandir}/man1/vfstest%{samba_major}*.1*
%exclude %{_mandir}/man1/log2pcap*.1*
%else
%exclude %{_mandir}/man1/vfstest%{samba_major}*.1*
%exclude %{_mandir}/man1/log2pcap*.1*
%endif

%if %build_system
%files -n %{libname}
%defattr(-,root,root)
%{_libdir}/libsmbclient.so.*
%else
%exclude %{_libdir}/libsmbclient.so.*
%endif

%if %build_system
%files -n %{libname}-devel
%defattr(-,root,root)
%{_includedir}/*
%{_libdir}/libsmbclient.so
%doc clean-docs/libsmbclient/*
%else
%exclude %{_includedir}/*
%exclude %{_libdir}/libsmbclient.so
%endif

%if %build_system
%files -n %{libname}-static-devel
%defattr(-,root,root)
%{_libdir}/libsmbclient.a
%else
%exclude %{_libdir}/libsmbclient.a
%endif

#%files passdb-ldap
#%defattr(-,root,root)
#%{_libdir}/%{name}/*/*ldap.so

%ifnarch alpha
%files passdb-mysql
%defattr(-,root,root)
%{_libdir}/%{name}/pdb/*mysql.so
%endif

#ifnarch alpha
%files passdb-pgsql
%defattr(-,root,root)
%{_libdir}/%{name}/pdb/*pgsql.so
#endif

%files passdb-xml
%defattr(-,root,root)
%{_libdir}/%{name}/pdb/*xml.so

#Files for antivirus support:
%if %build_clamav
%files vscan-clamav
%defattr(-,root,root)
%{_libdir}/%{name}/vfs/vscan-clamav.so
%config(noreplace) %{_sysconfdir}/%{name}/vscan-clamav.conf
%doc %{vfsdir}/%{vscandir}/INSTALL
%endif
%if !%build_clamav && %build_vscan
%exclude %{_libdir}/%{name}/vfs/vscan-clamav.so
%exclude %{_sysconfdir}/%{name}/vscan-clamav.conf
%endif

%if %build_fprot
%files vscan-fprot
%defattr(-,root,root)
%{_libdir}/%{name}/vfs/vscan-fprotd.so
%config(noreplace) %{_sysconfdir}/%{name}/vscan-fprotd.conf
%doc %{vfsdir}/%{vscandir}/INSTALL
%endif
%if !%build_fprot && %build_vscan
%exclude %{_libdir}/%{name}/vfs/vscan-fprotd.so
%exclude %{_sysconfdir}/%{name}/vscan-fprotd.conf
%endif

%if %build_fsav
%files vscan-fsecure
%defattr(-,root,root)
%{_libdir}/%{name}/vfs/vscan-fsav.so
%config(noreplace) %{_sysconfdir}/%{name}/vscan-fsav.conf
%doc %{vfsdir}/%{vscandir}/INSTALL
%endif
%if !%build_fsav && %build_vscan
%exclude %{_libdir}/%{name}/vfs/vscan-fsav.so
%exclude %{_sysconfdir}/%{name}/vscan-fsav.conf
%endif

%if %build_icap
%files vscan-icap
%defattr(-,root,root)
%{_libdir}/%{name}/vfs/vscan-icap.so
%config(noreplace) %{_sysconfdir}/%{name}/vscan-icap.conf
%doc %{vfsdir}/%{vscandir}/INSTALL
%endif
%if !%build_icap && %build_vscan
%exclude %{_libdir}/%{name}/vfs/vscan-icap.so
%exclude %{_sysconfdir}/%{name}/vscan-icap.conf
%endif


%if %build_kaspersky
%files vscan-kaspersky
%defattr(-,root,root)
%{_libdir}/%{name}/vfs/vscan-kavp.so
%config(noreplace) %{_sysconfdir}/%{name}/vscan-kavp.conf
%doc %{vfsdir}/%{vscandir}/INSTALL
%endif
%if !%build_kaspersky && %build_vscan
%exclude %{_libdir}/%{name}/vfs/vscan-kavp.so
%exclude %{_sysconfdir}/%{name}/vscan-kavp.conf
%endif

%if %build_mks
%files vscan-mks
%defattr(-,root,root)
%{_libdir}/%{name}/vfs/vscan-mksd.so
%config(noreplace) %{_sysconfdir}/%{name}/vscan-mks*.conf
%doc %{vfsdir}/%{vscandir}/INSTALL
%endif
%if !%build_mks && %build_vscan
%exclude %{_libdir}/%{name}/vfs/vscan-mksd.so
%exclude %{_sysconfdir}/%{name}/vscan-mks*.conf
%endif

%if %build_nai
%files vscan-nai
%defattr(-,root,root)
%{_libdir}/%{name}/vfs/vscan-mcdaemon.so
%config(noreplace) %{_sysconfdir}/%{name}/vscan-mcdaemon.conf
%doc %{vfsdir}/%{vscandir}/INSTALL
%endif
%if !%build_nai && %build_vscan
%exclude %{_libdir}/%{name}/vfs/vscan-mcdaemon.so
%exclude %{_sysconfdir}/%{name}/vscan-mcdaemon.conf
%endif

%if %build_openav
%files vscan-openav
%defattr(-,root,root)
%{_libdir}/%{name}/vfs/vscan-oav.so
%config(noreplace) %{_sysconfdir}/%{name}/vscan-oav.conf
%doc %{vfsdir}/%{vscandir}/INSTALL
%endif
%if !%build_openav && %build_vscan
%exclude %{_libdir}/%{name}/vfs/vscan-oav.so
%exclude %{_sysconfdir}/%{name}/vscan-oav.conf
%endif

%if %build_sophos
%files vscan-sophos
%defattr(-,root,root)
%{_libdir}/%{name}/vfs/vscan-sophos.so
%config(noreplace) %{_sysconfdir}/%{name}/vscan-sophos.conf
%doc %{vfsdir}/%{vscandir}/INSTALL
%endif
%if !%build_sophos && %build_vscan
%exclude %{_libdir}/%{name}/vfs/vscan-sophos.so
%exclude %{_sysconfdir}/%{name}/vscan-sophos.conf
%endif

%if %build_symantec
%files vscan-symantec
%defattr(-,root,root)
%{_libdir}/%{name}/vfs/vscan-symantec.so
%config(noreplace) %{_sysconfdir}/%{name}/vscan-symantec.conf
%doc %{vfsdir}/%{vscandir}/INSTALL
%endif
%if !%build_symantec && %build_vscan
%exclude %{_sysconfdir}/%{name}/vscan-symantec.conf
%endif

%if %build_trend
%files vscan-trend
%defattr(-,root,root)
%{_libdir}/%{name}/vfs/vscan-trend.so
%config(noreplace) %{_sysconfdir}/%{name}/vscan-trend.conf
%doc %{vfsdir}/%{vscandir}/INSTALL
%endif
%if !%build_trend && %build_vscan
%exclude %{_libdir}/%{name}/vfs/vscan-trend.so
%exclude %{_sysconfdir}/%{name}/vscan-trend.conf
%endif

%exclude %{_mandir}/man1/smbsh*.1*

%changelog
* Tue Nov 09 2004 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.8-1mdk
- 3.0.8
- add tdbtool to common
- fix doc permissions (broken in tarball)

* Fri Nov 05 2004 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.8-0.pre2.1mdk
- 3.0.8pre2

* Wed Oct 06 2004 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.7-3mdk
- drop patch 23 to ldap schema (patch in 3.0.6 was to bring schema up-to-date
  with pre-3.0.7 cvs) (#11960)
- merge winbind init script fix into packaging patch
- Don't set printcap name in pdf printer share (#11861)
- allow official builds off-cluster (with _with_official macro defined)

* Tue Sep 14 2004 Stew Benedict <sbenedict@mandrakesoft.com> 3.0.7-2mdk
- fix typo in winbind init script that prevented stop
  (in 3.0.6 too, patch27)

* Mon Sep 13 2004 Stew Benedict <sbenedict@mandrakesoft.com> 3.0.7-1mdk
- 3.0.7 (drop patch10,21,22,24,25,26; rediff patch23)

* Thu Sep 09 2004 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.6-5mdk
- patch for samba bug 1464
- make release-depenent release tag more like security updates tags
- sync smb.conf with drakwizard (which also fixes quoting of macros which
  can have spaces)
- add example admin share
- patches from Gerald Carter

* Mon Aug 31 2004 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.6-4mdk
- Add Jerry's post-3.0.6 patches
- fix bug 11088 

* Fri Aug 27 2004 Buchan Milne <bgmilne@linux-mandrake.com>3.0.6-3mdk
- patch from Urban Widmark via Robert Sim (anthill bug 1086) to be able
  to diable unix extensions in smbmount (and via 'unix extensions' in smb.conf)
- magic-devel only available on 9.2 and up
- allow building for stable release on the chroots on the cluster
- fix patch8
- fix build on older releases

* Fri Aug 20 2004 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.6-2mdk
- 3.0.6
-drop P6 (merged), P7 (broken for now)
-keep libsmbclient where it belongs (on x86 for now)
-implement mandrake version-specific release number

* Thu Aug 12 2004 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.5-2mdk
- 3.0.5
- fix pid file location (#10666)
- merge amd64 fixes (P7)
- make pdf printer work again, and other misc fixes to default config

* Sun Jun 20 2004 Oden Eriksson <oeriksson@mandrakesoft.com> 3.0.5-0.pre1.3mdk
- fix rpm group in libsmbclient0-devel (Goetz Waschk)

* Sat Jun 19 2004 Oden Eriksson <oeriksson@mandrakesoft.com> 3.0.5-0.pre1.2mdk
- fix deps

* Wed May 26 2004 Buchan Milne <bgmilne@linux-mandrake.com>3.0.5-0.pre1.1mdk
- fix building without scanners
- 3.0.5pre1 (and drop patch from CVS)

* Fri May 21 2004 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.4-3mdk
- re-work scanner support

* Thu May 13 2004 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.4-2mdk
- 3.0.4
- Patch for winbind (from samba bug 1315)

* Thu Apr 29 2004 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.2a-4mdk
- Fix samba-vscan (0.3.5), add clamav and icap, and build scanners by default
- Fix default vscan-clamav config and add sample config for homes share
- Add pgsql passdb backend

* Mon Mar 01 2004 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.2a-3mdk
- Fix default smbldap config
- Don't clobber smb.conf backup for no reason

* Mon Feb 16 2004 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.2a-2mdk
- 3.0.2a
- Only update smb.conf in upgrade from <3.0.1-3mdk (via trigger) and update
  upgrade script (stew)

* Mon Feb 09 2004 Buchan Milne <bgmilne@linux0mandrake.com> 3.0.2-2mdk
- 3.0.2

* Mon Feb 02 2004 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.2-0.rc2.1mdk
- 3.0.2rc2

* Tue Jan  6 2004 Stew Benedict <sbenedict@mandrakesoft.com> 3.0.1-5mdk
- update migrate script, feedback from Luca Berra

* Mon Jan  5 2004 Stew Benedict <sbenedict@mandrakesoft.com> 3.0.1-4mdk
- re-enable relaxed CFLAGS to fix broken smbmount, smbclient

* Fri Jan  2 2004 Stew Benedict <sbenedict@mandrakesoft.com> 3.0.1-3mdk
- add migrate script to merge existing smb.conf

* Fri Dec 19 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.1-2mdk
- 3.0.1 final

* Thu Dec 11 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.1-0.rc2.2mdk
- 3.0.1rc2

* Sat Dec 06 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.1-0.rc1.2mdk
- rc1
- samba-vscan-0.3.4

* Fri Dec 05 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.1-0.pre3.5mdk
- Allow winbind to start if old winbind ranges are used (ease upgrades)

* Tue Nov 18 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.1-0.pre3.4mdk
- Fix build as system on 8.2 (and probably earlier)

* Sun Nov 16 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.1-0.pre3.3mdk
- Ensure printer drivers keep permissions by default (setgid and inherit perms)

* Fri Nov 14 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.1-0.pre3.2mdk
- 3.0.1pre3
- Add support for Mandrake 10.0 (as system samba)
- Fix alternatives triggers
- Fix obsoletes

* Mon Nov 10 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.1-0.pre2.2mdk
- 3.0.1pre2
- misc spec files (pointed out by Luca Olivetti)
- Fix path to smbldap-passwd.pl
- Only allow one copy of winbind and nss_wins
- Add trigger for alternatives

* Sun Oct 12 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.1-0.pre1.2mdk
- 3.0.1pre1
- remove buildroot patch (p3), fixed upstream

* Thu Sep 25 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.0-2mdk
- 3.0.0 final

* Sat Sep 13 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.0-0.rc4.2mdk
- rc4
- Don't update alternatives in pre/post scripts when not using alternatives
- Fix case of --with-system without alternatives
- Final fixes to smbldap-tools for non-system case
- Remove duplicate docs (really - 1 character typo ...)
- Update configs (fix winbind init script, add example scripts in smb.conf)

* Tue Sep 09 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.0-0.rc3.2mdk
- rc3
- Fix mount.smb{,fs} alternatives (spotted by Laurent Culioli)

* Thu Sep 04 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.0-0.rc2.3mdk
- Fix alternatives
- Fix libname (can I blame guillomovitch's evil line-wrapping spec mode?)
- Fix smbldap-tools package/use names when not system samba
- Don't conflict samba3-client with samba-client for now so we can install it

* Fri Aug 29 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.0-0.rc2.2mdk
- rc2
- Remove patches 100-102 (upstream)
- Fix libname
- Alternatavise client
- Better solution to avoid rpath

* Fri Aug 22 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.0-0.rc1.3mdk
- Fix build with test package (p100), but not by default (too big)
- Fix (p101) for SID resolution when member of samba-2.2.x domain
- Fix libsmbclient packages (thanks Gotz)
- version mount.cifs, patch from CVS (p102), and setuid it
- Clean up docs (guillomovitch spam ;-)

* Sat Aug 16 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.0-0.rc1.2mdk
- rc1
- disable test subpackage since it's broken again

* Mon Jul 28 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.0-0.beta3.3mdk
- Rebuild for kerberos-1.3 on cooker
- Put printer directories back
- Add mount.cifs
- Go back to standard optimisations

* Thu Jul 17 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.0-0.beta3.2mdk
- beta3
- remove -g from cflags to avoid large static libraries
- drop optimisation from O2 to O1 for gcc 3.3.1
- own some directories for distriblint's benefit
- use chrpath on distro's that have it to drastically reduce rpmlint score

* Mon Jul 14 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.0-0.beta2.3mdk
- place non-conditional excludes at the end of files list, to prevent causing
  rpm in Mandrake <=8.2 from segfaulting when processing files.
- Update default config  

* Wed Jul 02 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.0-0.beta2.2mdk
- 3.0.0beta2
- manually build editreg
- Add some new man pages

* Tue Jun 10 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.0-0.beta1.3mdk
- add provision for passdb-ldap subpackage (it doesn't build like that yet)
- avoid debugging info on cooker/9.2 for the moment
- We probably don't need to autoconf (and can thus build on 8.1)
- We can probably build without kerberos support (and thus on 8.0)
- Don't require mysql-devel on alpha's (maybe we want to be able to disable
  mysql support for other arches?)
- We shouldn't need to specifically add openssl to include path, since ssl
  support is deprecated.
- png icons, change menu title to not conflict with ksambaplugin  
- update to samba-vscan-0.3.3beta1, but it still does not build the vscan
  modules.
- add -static-devel package
- Add buildrequires for lib packages that are picked up if installed
  (ncurses, popt) in an attempt to get slbd to build samba3
- Fix default config (P100)

* Sun Jun 08 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0.0-0.beta1.2mdk
- Get packages into cooker (klama doesn't want to build this package ..)
- samba-vscan-0.3.2b

* Fri Jun 06 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0-0.alpha24.2mdk
- Rename debug package to test and other fixes for rpm-4.2
- prepare for beta1

* Wed Apr 30 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0-0.alpha24.1mdk
- Remove some files removed upstream
- In builds from source, don't terminate on missing docs or unpackaged files
  (if only we could do it for other missing files ...)

* Mon Apr 28 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0-0.alpha24.0mdk
- Reenable debug package by (--without debug to not build it), fixed post-a23
- Add bugzilla note for builds from source (also intended for packages made
  available on samba FTP site) at samba team request
- Fix build from CVS (run autogen.sh, pass options to all rpm commands)
- Appease distriblint, but not much to be done about /usr/share/swat3/ since
  samba-doc owns some subdirs, and samba-swat others, and they can be installed
  independantly.
- Apply kaspersky vscan build fix from samba2  
- Final for alpha24

* Wed Apr 23 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0-0.alpha23.3mdk
- Small fixes in preparation for testing as system samba
- Make debug package optional (--with debug) since it's often broken
- Add support for 9.2 (including in-line smbd quota patch for glibc2.3)
- Add --with options option, which will just show you the available options and exit

* Sun Apr 06 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0-0.alpha23.2mdk
- Alpha23
- buildrequire autconf2.5
- samba-vscan 0.3.2a
- Remove patch 102 (upstreamed)

* Thu Mar 06 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0-0.alpha22.2mdk
- Alpha22
- Add profiles binary to server and ntlm_auth to common
- smbwrapper and torture target broken (only in 9.0?)
- remove unused source 2

* Tue Mar 04 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0-0.alpha21.4mdk
- Don't provide samba-{server,client,common} when not system samba (bug #2617)
- Don't build libsmbclient packages when not system samba
- Fix conflict between samba-server and samba3-server (pam_smbpass)
- Fix smbwrapper (from 2.2.7a-5mdk for bug #2356)
- Fix codepage/charset example (bug #1574)

* Thu Jan 23 2003 Buchan Milne <bgmilne@linux-mandrake.com> 3.0-0.alpha21.3mdk
- samba-vscan 0.3.1 (and make it build again), including required inline edits
- Make all vscan packages provide samba(3)-vscan
- Build all vscan except kav (requires kaspersky lib) with --with-scanners
- Add vscan-(scanner).conf files
- Explicitly add ldapsam for 2.2 compatability when building --with ldap,
  default build now uses new ldap passdb backend (ie you always get ldap)
- Enable (experimental) tdb passdb backend
- Fix file ownership conflicts between server and common
- Cleanup configure, to match order of --help
- Fix libdir location, was being overridden by --with-fhs
- Split off a libsmbclient and -devel package
- Add wins replication init script (patch 102)
- Workaround passdb/pdb_xml.c not compiling
- Workaround missing install targets for smbsh/smbwrapper.so in cvs
- Inline patch smbd/quotas.c for Mandrake >9.0

* Wed Nov 27 2002 Buchan Milne <bgmilne@linux-mandrake.com> 3.0-0.alpha21.2mdk
- Remove patch 20,21,22,23,25,26 (upstream)
- New destdir patch from cvs (18)
- package installed but non-packaged files
- new debug subpackage for vfstest and related files (it was that or nuke the 
  manpage ;-))
- use _libdir for libdir instead of _sysconfdir
- Update samba-vscan (untested)

* Mon Oct 28 2002 Buchan Milne <bgmilne@linux-mandrake.com> 3.0-0.alpha20.3mdk
- Fix mount.smbfs3 pointing to smbmount not in package
- Remove unnecessary lines from install (now done by make)
- Build with ldap and ads on all releases by default
- Put av-stuff back

* Mon Oct 21 2002 Buchan Milne <bgmilne@linux-mandrake.com> 3.0-0.alpha20.2mdk
- When not building as system samba, avoid conflicting with system samba
- Macro-ize as much as possible for above (aka finish cleanups)
- Fix paths in init scripts and logrotate and xinetd
- Fix provides and obsoletes so as to provide samba, but not obsolete
  current stable until we have a stable release (when it's the system samba).
- Add warnings to descriptions when not system samba.
- This is now parallel installable with the normal samba release, for easy
  testing. It shouldn't touch existing installations. Of course, only
  one samba at a time on the same interface!

* Sat Sep 28 2002 Buchan Milne <bgmilne@linux-mandrake.com> 3.0-0.alpha20.1mdk
- Merge with 2.2.6pre2.2mdk
- Detect alpha- and beta-, along with pre-releases

* Tue Feb 05 2002 Buchan Milne <bgmilne@cae.co.za> 3.0-alpha14-0.1mdk
- Sync with 2.2.3-2mdk (new --without options, detect when 
  building for a different distribution.

* Mon Feb 04 2002 Buchan Milne <bgmilne@cae.co.za> 3.0-alpha14-0.0mdk
- Sync with 2.2.2-10mdk, which added build-time options --with ldap,
  winbind, acl, wins, mdk72, mdk80, mdk81, mdk82, cooker. Added
  warning in description if built with these options.

* Wed Jan 23 2002 Buchan Milne <bgmilne@cae.co.za> 3.0-alpha13-0.2mdk
- Added if's for build_ads, which hopefully will add Active Directory
  Support (by request).

* Thu Jan 17 2002 Buchan Milne <bgmilne@cae.co.za> 3.0-alpha13-0.1mdk
- More syncing with 2.2 rpm (post and postun scripts)
- Testing without ldap

* Thu Jan 17 2002 Buchan Milne <bgmilne@cae.co.za> 3.0-alpha13-0.0mdk
- 3.0-alpha13
- Fixed installman.sh patch.

* Wed Jan 09 2002 Buchan Milne <bgmilne@cae.co.za> 3.0-alpha12-0.1mdk
- Fixed %post and %preun for nss_wins, added %post and %preun for
  samba-winbind (chkconfig and winbind entries in nsswitch.conf)

* Sun Dec 23 2001 Buchan Milne <bgmilne@cae.co.za> 3.0-alpha12-0.0mdk
- 3.0-alpha12
- Sync up with changes made in 2.2.2 to support Mandrake 8.0, 7.2
- Added new subpackage for swat
- More if's for ldap.

* Thu Dec 20 2001 Buchan Milne <bgmilne@cae.co.za> 3.0-alpha11-0.0mdk
- 3.0-alpha11

* Wed Dec 19 2001 Buchan Milne <bgmilne@cae.co.za> 3.0alpha10-0.0mdk
- 3.0-alpha10

* Tue Dec 18 2001 Buchan Milne <bgmilne@cae.co.za> 3.0alpha9-0.0mdk
- 3.0-alpha9

* Mon Dec 17 2001 Buchan Milne <bgmilne@cae.co.za> 3.0alpha8-0.1mdk
- Added net command to %files common, pdbedit and smbgroupedit to
  %files, s/%{prefix}\/bin/%{_bindir}/ (the big cleanup).
  Added patch to smb.init from 2.2.2 (got missed with 3.0-alpha1 patches)

* Sun Dec 16 2001 Buchan Milne <bgmilne@cae.co.za> 3.0alpha8-0.0mdk
- Patch for installman.sh to handle lang=en correctly (p24)
- added --with-manpages-langs=en,ja,pl (translated manpages), but there
  aren't any manpages for these languages yet ... so we still
  need %dir and %doc entries for them ...
- patch (p25) to configure.in to support more than 2 languages.
- addtosmbpass seems to have returned for now, but make_* have disappeared!

* Fri Dec 14 2001 Buchan Milne <bgmilne@cae.co.za> 3.0alpha6-0.0mdk
- DESTDIR patch for Makefile.in (p23), remove a lot of %%install scripts
  this forces move of smbcontrol and smbmnt to %{prefix}/bin
  removed --with-pam_smbpass as it doesn't compile.

* Thu Dec 06 2001 Buchan Milne <bgmilne@cae.co.za> 3.0-0.0alpha1mdk
- Samba 3.0alpha1 released (we missed Samba 3.0alpha0!)
- Redid smbmount-sbin patch and smb.conf patch (20), removed xfs quota patch 
  (applied upstream), removed ook-patch (codepage directory totally different).
- Added winbind.init (21) and system-auth-winbind.pamd (22). Patches 20-23 
  should be applied upstream before 3.0 ships ...

* Wed Dec 05 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.2-6mdk
- fixed typo in system-auth-winbind.pamd (--Thanks J. Gluck).
- fixed %post xxx problem (smb not started in chkconfig --Thanks Viet & B. Kenworthy).

* Fri Nov 23 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.2-5mdk
- Had to remove the network recycle bin patch: it seems to mess up 
  file deletion from windows (files appear to be "already in use")

* Tue Nov 13 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.2-4mdk
- added network recycle bin patch:
  <http://www.amherst.edu/~bbstone/howto/samba.html>
- added "recycle bin = .recycled" parameter in smb.conf [homes].
- fixed winbind/nss_wins perms (oh no I don't own that stuff ;o)

* Mon Nov 12 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.2-3mdk
- added %build 8.0 and 7.2, for tweakers to play around.
- changed configure options:
  . removed --with-mmap, --with-netatalk (obsolete).
  . added --with-msdfs, --with-vfs (seems stable, but still need testing).

* Mon Nov 12 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.2-2mdk
- rebuilt with winbind and nss_wins enabled.

* Wed Oct 31 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.2-1mdk
- Rebuilt on cooker.

* Wed Oct 31 2001 Buchan Milne <bgmilne@cae.co.za> 2.2.2-0.992mdk
- Patch for smb.conf to fix incorrect lpq command, typo in winbind,
  and add sample linpopup command. Added print driver directories.
- New XFS quota patch (untested!, samba runs, but do quotas work? We
  can't check yet since the kernel doesn't seem to support XFS quotas!)

* Fri Oct 19 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.2-0.99mdk
- New samba.spec, almost ready for winbind operations. OLA for Buchan Milne
  Who did a tremendous integration work on 2.2.2.
  Rebuild on cooker, please test XFS (ACLs and quotas) again...
  
* Mon Oct 15 2001 Buchan Milne <bgmilne@cae.co.za> 2.2.2-0.9mdk
- Samba-2.2.2. released! Use %defines to determine which subpackages
  are built and which Mandrake release we are buiding on/for (hint: define 
  build_mdk81 1 for Mandrake 8.1 updates)

* Sun Oct 14 2001 Buchan Milne <bgmilne@cae.co.za> 2.2.2-0.20011014mdk
- %post and %postun for nss_wins

* Wed Oct 10 2001 Buchan Milne <bgmilne@cae.co.za> 2.2.2-0.20011010mdk
- New CVS snapshot, /etc/pam.d/system-auth-winbind added
  with configuration to allow easy winbind setup.
  
* Sun Oct 7 2001 Buchan Milne <bgmilne@cae.co.za> 2.2.2-0.20011007mdk
- Added new package nss_wins and moved smbpasswd to common (required by
  winbind).

* Sat Oct 6 2001 Buchan Milne <bgmilne@cae.co.za> 2.2.2-0.20011006mdk
- Added new package winbind.

* Mon Oct 1 2001 Buchan Milne <bgmilne@cae.co.za> 2.2.2-0.20011001mdk
- Removed patch to smb init.d file (applied in cvs)

* Sun Sep 30 2001 Buchan Milne <bgmilne@cae.co.za> 2.2.2-0.20010930mdk
- Added winbind init script, which still needs to check for running nmbd.

* Thu Sep 27 2001 Buchan Milne <bgmilne@cae.co.za> 2.2.2-0.20010927mdk
- Built from samba-2.2.2-pre cvs, added winbindd, wbinfo, nss_winbind and 
  pam_winbind, moved pam_smbpass from samba-common to samba. We still
  need a start-up script for winbind, or need to modify existing one.
  
* Mon Sep 10 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.1a-15mdk
- Enabled acl support (XFS acls now supported by kernel-2.4.8-21mdk thx Chmou)
  Added smbd patch to support XFS quota (Nathan Scott)
  
* Mon Sep 10 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.1a-14mdk
- Oops! smbpasswd created in wrong directory...

* Tue Sep 06 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.1a-13mdk
- Removed a wrong comment in smb.conf.
  Added creation of smbpasswd during install.

* Mon Aug 27 2001 Pixel <pixel@mandrakesoft.com> 2.2.1a-12mdk
- really less verbose %%post

* Sat Aug 25 2001 Geoffrey Lee <snailtalk@mandrakesoft.com> 2.2.1a-11mdk
- Fix shared libs in /usr/bin silliness.

* Thu Aug 23 2001 Pixel <pixel@mandrakesoft.com> 2.2.1a-10mdk
- less verbose %%post

* Wed Aug 22 2001 Buchan Milne <bgmilne@cae.co.za> 2.2.1a-9mdk
- Added smbcacls (missing in %files), modification to smb.conf: ([printers]
  is still needed, even with point-and-print!, user add script should
  use name and not gid, since we may not get the gid . New script for
  putting manpages in place (still need to be added in %files!). Moved
  smbcontrol to sbin and added it and its man page to %files.

* Wed Aug 22 2001 Pixel <pixel@mandrakesoft.com> 2.2.1a-8mdk
- cleanup /var/lib/samba/codepage/src

* Tue Aug 21 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.1a-7mdk
- moved codepage generation to %%install and codepage dir to /var/lib/samba

* Tue Aug 21 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.1a-6mdk
- /lib/* was in both samba and samba-common
  Introducing samba-doc: "alas, for the sake of thy modem, shalt thou remember
  when Samba was under the Megabyte..."

* Fri Aug 03 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.1a-5mdk
- Added "the gc touch" to smbinit through the use of killall -0 instead of
  grep cupsd | grep -v grep (too many greps :o)

* Wed Jul 18 2001 Stefan van der Eijk <stefan@eijk.nu> 2.2.1a-4mdk
- BuildRequires: libcups-devel
- Removed BuildRequires: openssl-devel

* Fri Jul 13 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.1a-3mdk
- replace chkconfig --add/del with --level 35 on/reset.

* Fri Jul 13 2001 Geoffrey Lee <snailtalk@mandrakesoft.cm> 2.2.1a-2mdk
- Replace discription s/inetd/xinetd/, we all love xinetd, blah.

* Thu Jul 12 2001 Buchan Milne <bgmilne@cae.co.za> 2.2.1a-1mdk
- Bugfix release. Fixed add user script, added print$ share and printer admin
  We need to test interaction of new print support with CUPS, but printer
  driver uploads should work.

* Wed Jul 11 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.1-17mdk
- fixed smb.conf a bit, rebuilt on cooker.

* Tue Jul 10 2001 Buchan Milne <bgmilne@cae.co.za> 2.2.1-16mdk
- Finally, samba 2.2.1 has actually been release. At least we were ready!
  Cleaned up smb.conf, and added some useful entries for domain controlling.
  Migrated changes made in samba's samba2.spec for 2.2.1  to this file.
  Added groupadd command in post to create a group for samba machine accounts.
  (We should still check the postun, samba removes pam, logs and cache)

* Tue Jun 26 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.1-15mdk
- fixed smbwrapper compile options.

* Tue Jun 26 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.1-14mdk
- added LFS support.
  added smbwrapper support (smbsh)

* Wed Jun 20 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.1-13mdk
- /sbin/mount.smb and /sbin/mount.smbfs now point to the correct location
  of smbmount (/usr/bin/smbmount)

* Tue Jun 19 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.1-12mdk
- smbmount and smbumount are now in /usr/bin and SUID.
  added ||: to triggerpostun son you don't get error 1 anymore when rpm -e
  Checked the .bz2 sources with file *: everything is OK now (I'm so stupid ;o)!

* Tue Jun 19 2001 Geoffrey Lee <snailtalk@mandrakesoft.com> 2.2.1-11mdk
- s/Copyright/License/;
- Stop Sylvester from pretending .gz source to be .bz2 source via filename
  aka really bzip2 the source.

* Mon Jun 18 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.1-10mdk
- changed Till's startup script modifications: now samba is being reloaded
  automatically 1 minute after it has started (same reasons as below in 9mdk)
  added _post_ and _preun_ for service smb
  fixed creation of /var/lib/samba/{netlogon,profiles} (%dir was missing)

* Thu Jun 14 2001 Till Kamppeter <till@mandrakesoft.com> 2.2.1-9mdk
- Modified the Samba startup script so that in case of CUPS being used as
  printing system Samba only starts when the CUPS daemon is ready to accept
  requests. Otherwise the CUPS queues would not appear as Samba shares.

* Mon Jun 11 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.1-8mdk
- patched smbmount.c to have it call smbmnt in sbin (thanks Seb).

* Wed May 30 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.1-7mdk
- put SWAT menu icons back in place.

* Mon May 28 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.1-6mdk
- OOPS! fixed smbmount symlinks

* Mon May 28 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.1-5mdk
- removed inetd postun script, replaced with xinetd.
  updated binary list (smbcacls...)
  cleaned samba.spec

* Mon May 28 2001 Buchan Milne <bgmilne@cae.co.za> 2.2.1-4mdk
- Changed configure options to point to correct log and codepage directories,
  added crude script to fix logrotate file for new log file names, updated
  patches to work with current CVS.

* Thu May 24 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.1-3mdk
- Cleaned and updated the %files section.

* Sat May 19 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.1-2mdk
- Moved all samba files from /etc to /etc/samba (Thanks DomS!).
  Fixed fixinit patch (/etc/samba/smb.conf)

* Fri May 18 2001 Buchan Milne <bgmilne@cae.co.za> 2.2.1-1mdk
- Now use packaging/Mandrake/smb.conf, removed unused and obsolete
  patches, moved netlogon and profile shares to /var/lib/samba in the
  smb.conf to match the spec file. Added configuration for ntlogon to
  smb.conf. Removed pam-foo, fixinit and makefilepath patches. Removed
  symlink I introduced in 2.2.0-1mdk

* Thu May 3 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.0-5mdk
- Added more configure options. Changed Description field (thx John T).

* Wed Apr 25 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.0-4mdk
- moved netlogon and profiles to /var/lib/samba by popular demand ;o)

* Tue Apr 24 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.0-3mdk
- moved netlogon and profiles back to /home.

* Fri Apr 20 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.2.0-2mdk
- fixed post inetd/xinetd script&

* Thu Apr 19 2001 Buchan Milne <bgmilne@cae.co.za> 2.2.0-1mdk
- Upgrade to 2.2.0. Merged most of 2.0.7-25mdk's patches (beware
  nasty "ln -sf samba-%{ver} ../samba-2.0.7" hack to force some patches
  to take. smbadduser and addtosmbpass seem to have disappeared. Moved
  all Mandrake-specific files to packaging/Mandrake and made patches
  from those shipped with samba. Moved netlogon to /home/samba and added
  /home/samba/profiles. Added winbind,smbfilter and debug2html to make command.

* Thu Apr 12 2001 Frederic Crozat <fcrozat@mandrakesoft.com> 2.0.7-25mdk
- Fix menu entry and provide separate menu entry for GNOME
  (nautilus doesn't support HTTP authentication yet)
- Add icons in package

* Fri Mar 30 2001 Frederic Lepied <flepied@mandrakesoft.com> 2.0.7-24mdk
- use new server macros

* Wed Mar 21 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.0.7-23mdk
- check whether /etc/inetd.conf exists (upgrade) or not (fresh install).

* Thu Mar 15 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.0.7-22mdk
- spec cosmetics, added '-r' option to lpr-cups command line so files are
  removed from /var/spool/samba after printing.

* Tue Mar 06 2001 Sylvestre Taburet <staburet@mandrakesoft.com> 2.0.7-21mdk
- merged last rh patches.

* Thu Nov 23 2000 Sylvestre Taburet <staburet@mandrakesoft.com> 2.0.7-20mdk
- removed dependencies on cups and cups-devel so one can install samba without using cups
- added /home/netlogon

* Mon Nov 20 2000 Till Kamppeter <till@mandrakesoft.com> 2.0.7-19mdk
- Changed default print command in /etc/smb.conf, so that the Windows
  driver of the printer has to be used on the client.
- Fixed bug in smbspool which prevented from printing from a
  Linux-Samba-CUPS client to a Windows server through the guest account.

* Mon Oct 16 2000 Till Kamppeter <till@mandrakesoft.com> 2.0.7-18mdk
- Moved "smbspool" (Samba client of CUPS) to the samba-client package

* Sat Oct 7 2000 Stefan van der Eijk <s.vandereijk@chello.nl> 2.0.7-17mdk
- Added RedHat's "quota" patch to samba-glibc21.patch.bz2, this fixes
  quota related compile problems on the alpha.

* Wed Oct 4 2000 Sylvestre Taburet <staburet@mandrakesoft.com> 2.0.7-16mdk
- Fixed 'guest ok = ok' flag in smb.conf

* Tue Oct 3 2000 Sylvestre Taburet <staburet@mandrakesoft.com> 2.0.7-15mdk
- Allowed guest account to print in smb.conf
- added swat icon in menu

* Tue Oct 3 2000 Sylvestre Taburet <staburet@mandrakesoft.com> 2.0.7-14mdk
- Removed rh ssl patch and --with-ssl flag: not appropriate for 7.2

* Tue Oct 3 2000 Sylvestre Taburet <staburet@mandrakesoft.com> 2.0.7-13mdk
- Changed fixinit patch.
- Changed smb.conf for better CUPS configuration.
- Thanks Fred for doing this ---vvv.

* Tue Oct  3 2000 Frederic Lepied <flepied@mandrakesoft.com> 2.0.7-12mdk
- menu entry for web configuration tool.
- merge with rh: xinetd + ssl + pam_stack.
- Added smbadduser rh-bugfix w/o relocation of config-files.

* Mon Oct  2 2000 Frederic Lepied <flepied@mandrakesoft.com> 2.0.7-11mdk
- added build requires on cups-devel and pam-devel.

* Mon Oct  2 2000 Till Kamppeter <till@mandrakesoft.com> 2.0.7-10mdk
- Fixed smb.conf entry for CUPS: "printcap name = lpstat", "lpstats" was
  wrong.

* Mon Sep 25 2000 Sylvestre Taburet <staburet@mandrakesoft.com> 2.0.7-9mdk
- Cosmetic changes to make rpmlint more happy

* Wed Sep 11 2000 Sylvestre Taburet <staburet@mandrakesoft.com> 2.0.7-8mdk
- added linkage to the using_samba book in swat

* Fri Sep 01 2000 Sylvestre Taburet <staburet@mandrakesoft.com> 2.0.7-7mdk
- Added CUPS support to smb.conf
- Added internationalization options to smb.conf [Global]

* Wed Aug 30 2000 Till Kamppeter <till@mandrakesoft.com> 2.0.7-6mdk
- Put "smbspool" to the files to install

* Wed Aug 30 2000 Sylvestre Taburet <staburet@mandrakesoft.com> 2.0.7-5mdk
- Did some cleaning in the patches

* Fri Jul 28 2000 Sylvestre Taburet <staburet@mandrakesoft.com> 2.0.7-4mdk
- relocated man pages from /usr/man to /usr/share/man for compatibility reasons

* Fri Jul 28 2000 Sylvestre Taburet <staburet@mandrakesoft.com> 2.0.7-3mdk
- added make_unicodemap and build of unicode_map.$i in the spec file

* Fri Jul 28 2000 Sylvestre Taburet <staburet@mandrakesoft.com> 2.0.7-2mdk
- renamed /etc/codepage/codepage.$i into /etc/codepage/unicode_map.$i to fix smbmount bug.

* Fri Jul 07 2000 Sylvestre Taburet <staburet@mandrakesoft.com> 2.0.7-1mdk
- 2.0.7

* Wed Apr 05 2000 Francis Galiegue <fg@mandrakesoft.com> 2.0.6-4mdk

- Titi sucks, does not put versions in changelog
- Fixed groups for -common and -client
- /usr/sbin/samba is no config file

* Thu Mar 23 2000 Thierry Vignaud <tvignaud@mandrakesoft.com>
- fix buggy post install script (pixel)

* Fri Mar 17 2000 Francis Galiegue <francis@mandrakesoft.com> 2.0.6-2mdk

- Changed group according to 7.1 specs
- Some spec file changes
- Let spec-helper do its job

* Thu Nov 25 1999 Chmouel Boudjnah <chmouel@mandrakesoft.com>
- 2.0.6.

* Tue Nov  2 1999 Chmouel Boudjnah <chmouel@mandrakesoft.com>
- Merge with rh changes.
- Split in 3 packages.

* Fri Aug 13 1999 Pablo Saratxaga <pablo@@mandrakesoft.com>
- corrected a bug with %post (the $1 parameter is "1" in case of
  a first install, not "0". That parameter is the number of packages
  of the same name that will exist after running all the steps if nothing
  is removed; so it is "1" after first isntall, "2" for a second install
  or an upgrade, and "0" for a removal)

* Wed Jul 28 1999 Pablo Saratxaga <pablo@@mandrakesoft.com>
- made smbmnt and smbumount suid root, and only executable by group 'smb'
  add to 'smb' group any user that should be allowed to mount/unmount
  SMB shared directories

* Fri Jul 23 1999 Chmouel Boudjnah <chmouel@mandrakesoft.com>
- 2.0.5a (bug security fix).

* Wed Jul 21 1999 Axalon Bloodstone <axalon@linux-mandrake.com>
- 2.0.5
- cs/da/de/fi/fr/it/tr descriptions/summaries

* Sun Jun 13 1999 Bernhard Rosenkränzer <bero@mandrakesoft.com>
- 2.0.4b
- recompile on a system that works ;)

* Wed Apr 21 1999 Chmouel Boudjnah <chmouel@mandrakesoft.com>
- Mandrake adaptations.
- Bzip2 man-pages.

* Fri Mar 26 1999 Bill Nottingham <notting@redhat.com>
- add a mount.smb to make smb mounting a little easier.
- smb filesystems apparently do not work on alpha. Oops.

* Thu Mar 25 1999 Bill Nottingham <notting@redhat.com>
- always create codepages

* Tue Mar 23 1999 Bill Nottingham <notting@redhat.com>
- logrotate changes

* Sun Mar 21 1999 Cristian Gafton <gafton@redhat.com>
- auto rebuild in the new build environment (release 3)

* Fri Mar 19 1999 Preston Brown <pbrown@redhat.com>
- updated init script to use graceful restart (not stop/start)

* Tue Mar  9 1999 Bill Nottingham <notting@redhat.com>
- update to 2.0.3

* Thu Feb 18 1999 Bill Nottingham <notting@redhat.com>
- update to 2.0.2

* Mon Feb 15 1999 Bill Nottingham <notting@redhat.com>
- swat swat

* Tue Feb  9 1999 Bill Nottingham <notting@redhat.com>
- fix bash2 breakage in post script

* Fri Feb  5 1999 Bill Nottingham <notting@redhat.com>
- update to 2.0.0

* Mon Oct 12 1998 Cristian Gafton <gafton@redhat.com>
- make sure all binaries are stripped

* Thu Sep 17 1998 Jeff Johnson <jbj@redhat.com>
- update to 1.9.18p10.
- fix %triggerpostun.

* Tue Jul 07 1998 Erik Troan <ewt@redhat.com>
- updated postun triggerscript to check $0
- clear /etc/codepages from %preun instead of %postun

* Mon Jun 08 1998 Erik Troan <ewt@redhat.com>
- made the %postun script a tad less agressive; no reason to remove
  the logs or lock file (after all, if the lock file is still there,
  samba is still running)
- the %postun and %preun should only exectute if this is the final
  removal
- migrated %triggerpostun from Red Hat's samba package to work around
  packaging problems in some Red Hat samba releases

* Sun Apr 26 1998 John H Terpstra <jht@samba.anu.edu.au>
- minor tidy up in preparation for release of 1.9.18p5
- added findsmb utility from SGI package

* Wed Mar 18 1998 John H Terpstra <jht@samba.anu.edu.au>
- Updated version and codepage info.
- Release to test name resolve order

* Sat Jan 24 1998 John H Terpstra <jht@samba.anu.edu.au>
- Many optimisations (some suggested by Manoj Kasichainula <manojk@io.com>
- Use of chkconfig in place of individual symlinks to /etc/rc.d/init/smb
- Compounded make line
- Updated smb.init restart mechanism
- Use compound mkdir -p line instead of individual calls to mkdir
- Fixed smb.conf file path for log files
- Fixed smb.conf file path for incoming smb print spool directory
- Added a number of options to smb.conf file
- Added smbadduser command (missed from all previous RPMs) - Doooh!
- Added smbuser file and smb.conf file updates for username map
