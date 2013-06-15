#!/usr/bin/perl -w

# $Id: configure.pl,v 1.17 2005/07/05 09:05:16 jtournier Exp $
# $Source: /opt/cvs/samba/smbldap-tools/configure.pl,v $

# This script can help you setting up the smbldap_conf.pl file. It will get all the defaults value
# that are defined in the smb.conf configuration file. You should then start with this configuration
# file. You will also need the SID for your samba domain: set up the controler domain before using
# this script.

#  This code was developped by IDEALX (http://IDEALX.org/) and
#  contributors (their names can be found in the CONTRIBUTORS file).
#
#                 Copyright (C) 2002 IDEALX
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
#  USA.


use strict;
use File::Basename;

# we need to be root to configure the scripts
if ($< != 0) {
  die "Only root can configure the smbldap-tools scripts\n";
}

print "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
       smbldap-tools script configuration
       -=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
Before starting, check
 . if your samba controller is up and running.
 . if the domain SID is defined (you can get it with the 'net getlocalsid')

 . you can leave the configuration using the Crtl-c key combination
 . empty value can be set with the \".\" character\n";
print "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-\n";

# we first check if Samba is up and running
my $test_smb=`pidof smbd`;
chomp($test_smb);
die "\nSamba need to be started first !\n" if ($test_smb eq "" || not defined $test_smb);

print "Looking for configuration files...\n\n";
my $smb_conf="";
if (-e "/etc/samba/smb.conf") {
  $smb_conf="/etc/samba/smb.conf";
} elsif (-e "/usr/local/samba/lib/smb.conf") {
  $smb_conf="/usr/local/samba/lib/smb.conf";
}
print "Samba Configuration File Path [$smb_conf] > ";
chomp(my $config_smb=<STDIN>);
if ($config_smb ne "") {
  $smb_conf=$config_smb;
}

my $conf_dir;
if (-d "/etc/opt/IDEALX/smbldap-tools") {
	$conf_dir="/etc/opt/IDEALX/smbldap-tools/";
} elsif (-d "/etc/smbldap-tools") {
	$conf_dir="/etc/smbldap-tools/";
} else {
	$conf_dir="/etc/opt/IDEALX/smbldap-tools/";
}

print "\nThe default directory in which the smbldap configuration files are stored is shown.\n";
print "If you need to change this, enter the full directory path, then press enter to continue.\n";
print "Smbldap-tools Configuration Directory Path [$conf_dir] > ";
my $conf_dir_tmp;
chomp($conf_dir_tmp=<STDIN>);
if ($conf_dir_tmp ne "") {
  $conf_dir=$conf_dir_tmp;
}

$conf_dir=~s/(\w)$/$1\//;
if (! -d $conf_dir) {
	mkdir "$conf_dir";
}

my $smbldap_conf="$conf_dir"."smbldap.conf";
my $smbldap_bind_conf="$conf_dir"."smbldap_bind.conf";



# Let's read the smb.conf configuration file
my %config;
open (CONFIGFILE, "$smb_conf") || die "Unable to open $smb_conf for reading !\n";

while (<CONFIGFILE>) {

  chomp($_);

  ## eat leading whitespace
  $_=~s/^\s*//;

  ## eat trailing whitespace
  $_=~s/\s*$//;


  ## throw away comments
  next if (($_=~/^#/) || ($_=~/^;/));

  ## check for a param = value
  if ($_=~/=/) {
    #my ($param, $value) = split (/=/, $_);
    my ($param, $value) = ($_=~/([^=]*)=(.*)/i);
    $param=~s/./\l$&/g;
    $param=~s/\s+//g;
    $value=~s/^\s+//;

    $value=~s/"//g;

    $config{$param} = $value;
    #print "param=$param\tvalue=$value\n";

    next;
  }
}
close (CONFIGFILE);

print "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=\n";
print "Let's start configuring the smbldap-tools scripts ...\n\n";

# This function need 4 parameters:
# . the description of the parameter
# . name of the key it is related to in the %config hash (key similar as the name parameter in
#   smb.conf). You can get all the available keys using this:
#   foreach my $tmp (keys %config) {
#	print "key=$tmp\t value=$config{$tmp}\n";
#   }
# . if no value is found in smb.conf for the keys, this value is proposed
# . the 'insist' variable: if set to 1, then the script will always call for a value
#   for the parameter. In other words, there's not default value, and it can't be set
#   to a null caracter string.

sub read_entry
  {
    my $description=shift;
    my $value=shift;
    my $example_value=shift;
    my $insist=shift;
    my $value_tmp;
    chomp($value);
    $insist=0 if (! defined $insist);
    if (defined $config{$value} and $config{$value} ne "") {
      print "$description [$config{$value}] > ";
      $value_tmp=$config{$value};
    } else {
      print "$description [$example_value] > ";
      $value_tmp="$example_value";
    }
    chomp(my $get=<STDIN>);
    if ($get eq "") {
      $value=$value_tmp;
    } elsif ($get eq ".") {
      $value="";
    } else {
      $value=$get;
    }
    if ($insist == 1 and "$value" eq "") {
      while ($insist == 1) {
	print "  Warning: You really need to set this parameter...\n";
	$description=~s/. /  /;
	if (defined $config{$value}) {
	  print "$description [$config{$value}] > ";
	  $value_tmp=$config{$value};
	} else {
	  print "$description [$value] > ";
	  $value_tmp="$value";
	}
	chomp(my $get=<STDIN>);
	if ($get eq "") {
	  $value=$value_tmp;
	} elsif ($get eq ".") {
	  $value="";
	} else {
	  $value=$get;
	  $insist=0;
	}
      }
    }
    return $value;
  }

print ". workgroup name: name of the domain Samba act as a PDC\n";
my $workgroup=read_entry("  workgroup name","workgroup","",0);

print ". netbios name: netbios name of the samba controler\n";
my $netbios_name=read_entry("  netbios name","netbiosname","",0);

print ". logon drive: local path to which the home directory will be connected (for NT Workstations). Ex: 'H:'\n";
my $logondrive=read_entry("  logon drive","logondrive","",0);

print ". logon home: home directory location (for Win95/98 or NT Workstation).\n  (use %U as username) Ex:'\\\\$netbios_name\\%U'\n";
my $logonhome=read_entry("  logon home (press the \".\" character if you don't want homeDirectory)","logonhome","\\\\$netbios_name\\%U",0);
#$logonhome=~s/\\/\\\\/g;

print ". logon path: directory where roaming profiles are stored. Ex:'\\\\$netbios_name\\profiles\\\%U'\n";
my $logonpath=read_entry("  logon path (press the \".\" character if you don't want roaming profile)","logonpath","\\\\$netbios_name\\profiles\\\%U",0);
#$logonpath=~s/\\/\\\\/g;

my $userHome=read_entry(". home directory prefix (use %U as username)","","/home/\%U",0);

my $userHomeDirectoryMode=read_entry(". default users' homeDirectory mode","","700",0);

my $userScript=read_entry(". default user netlogon script (use %U as username)","logonscript","",0);

my $defaultMaxPasswordAge=read_entry("  default password validation time (time in days)","","45",0);

#############################
# ldap directory parameters #
#############################
my $ldap_suffix=read_entry(". ldap suffix","ldapsuffix","",0);
my $ldap_group_suffix=read_entry(". ldap group suffix","ldapgroupsuffix","",0);
$ldap_group_suffix=~s/ou=//;
my $ldap_user_suffix=read_entry(". ldap user suffix","ldapusersuffix","",0);
$ldap_user_suffix=~s/ou=//;
my $ldap_machine_suffix=read_entry(". ldap machine suffix","ldapmachinesuffix","",0);
$ldap_machine_suffix=~s/ou=//;
my $ldap_idmap_suffix=read_entry(". Idmap suffix","ldapidmapsuffix","ou=Idmap",0);
print ". sambaUnixIdPooldn: object where you want to store the next uidNumber\n";
print "  and gidNumber available for new users and groups\n";
my $sambaUnixIdPooldn=read_entry("  sambaUnixIdPooldn object (relative to \${suffix})","","sambaDomainName=$workgroup",0);

# parameters for the master ldap server
my ($trash1,$server);
if (defined $config{passdbbackend}) {
  ($trash1,$server)=($config{passdbbackend}=~m/(.*)ldap:\/\/(.*)/);
} else {
  $server="127.0.0.1";
}
$server=~s/\///;
my $ldapmasterserver;
print ". ldap master server: IP adress or DNS name of the master (writable) ldap server\n";
$ldapmasterserver=read_entry("  ldap master server","",$server,0);
my $ldapmasterport;
if (defined $config{ldapport}) {
  $ldapmasterport=read_entry(". ldap master port","ldapport","",0);
} else {
  $ldapmasterport=read_entry(". ldap master port","","389",0);
}
my $ldap_master_admin_dn=read_entry(". ldap master bind dn","ldapadmindn","",0);
system "stty -echo";
my $ldap_master_bind_password=read_entry(". ldap master bind password","","",1);
print "\n";
system "stty echo";

# parameters for the slave ldap server
print ". ldap slave server: IP adress or DNS name of the slave ldap server: can also be the master one\n";
my $ldap_slave_server=read_entry("  ldap slave server","","$server",0);
my $ldap_slave_port;
if (defined $config{ldapport}) {
  $ldap_slave_port=read_entry(". ldap slave port","ldapport","",0);
} else {
  $ldap_slave_port=read_entry(". ldap slave port","","389",0);
}
my $ldap_slave_admin_dn=read_entry(". ldap slave bind dn","ldapadmindn","",0);
system "stty -echo";
my $ldap_slave_bind_password=read_entry(". ldap slave bind password","","",1);
print "\n";
system "stty echo";
my $ldaptls=read_entry(". ldap tls support (1/0)","","0",0);
my ($cert_verify,$cert_cafile,$cert_clientcert,$cert_clientkey)=("","","","");
if ($ldaptls == 1) {
  $cert_verify=read_entry(". How to verify the server's certificate (none, optional or require)","","require",0);
  $cert_cafile=read_entry(". CA certificate file","","$conf_dir/ca.pem",0);
  $cert_clientcert=read_entry(". certificate to use to connect to the ldap server","","$conf_dir/smbldap-tools.pem",0);
  $cert_clientkey=read_entry(". key certificate to use to connect to the ldap server","","$conf_dir/smbldap-tools.key",0);
}

# let's test if any sid is available
# Here is the strategy: If smb.conf has 'domain master = No'
#  this means we are a BDC and we must obtain the SID from the PDC
#  using the command 'net rpc getsid -S PDC -Uroot%password' BEFORE
#  executing this script - that then guarantees the correct SID is available.
my $sid_tmp=`net getlocalsid \$netbios_name 2>/dev/null | cut -f2 -d: | sed "s/ //g"`;
chomp $sid_tmp;
print ". SID for domain $config{workgroup}: SID of the domain (can be obtained with 'net getlocalsid $netbios_name')\n";
my $sid=read_entry("  SID for domain $config{workgroup}","","$sid_tmp",0);

print ". unix password encryption: encryption used for unix passwords\n";
my $cryp_algo=read_entry("  unix password encryption (CRYPT, MD5, SMD5, SSHA, SHA)","","SSHA",0);
my $crypt_salt_format="";
if ( $cryp_algo eq "CRYPT" ) {
  print ". crypt salt format: If hash_encrypt is set to CRYPT, you may set \n";
  print "  a salt format. The default is \"\%s\", but many systems will generate\n";
  print "  MD5 hashed passwords if you use \"\$1\$\%\.8s\"\n";
  $crypt_salt_format=read_entry("  crypt salt format","","\%s",0);
}

my $default_user_gidnumber=read_entry(". default user gidNumber","","513",0);

my $default_computer_gidnumber=read_entry(". default computer gidNumber","","515",0);

my $userLoginShell=read_entry(". default login shell","","/bin/bash",0);

my $skeletonDir=read_entry(". default skeleton directory","","/etc/skel",0);

my $mailDomain=read_entry(". default domain name to append to mail adress", "","",0);

print "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=\n";
my $template_smbldap="
# \$Source: /opt/cvs/samba/smbldap-tools/configure.pl,v $
# \$Id: configure.pl,v 1.17 2005/07/05 09:05:16 jtournier Exp $
#
# smbldap-tools.conf : Q & D configuration file for smbldap-tools

#  This code was developped by IDEALX (http://IDEALX.org/) and
#  contributors (their names can be found in the CONTRIBUTORS file).
#
#                 Copyright (C) 2001-2002 IDEALX
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
#  USA.

#  Purpose :
#       . be the configuration file for all smbldap-tools scripts

##############################################################################
#
# General Configuration
#
##############################################################################

# Put your own SID. To obtain this number do: \"net getlocalsid\".
# If not defined, parameter is taking from \"net getlocalsid\" return
SID=\"$sid\"

# Domain name the Samba server is in charged.
# If not defined, parameter is taking from smb.conf configuration file
# Ex: sambaDomain=\"IDEALX-NT\"
sambaDomain=\"$workgroup\"

##############################################################################
#
# LDAP Configuration
#
##############################################################################

# Notes: to use to dual ldap servers backend for Samba, you must patch
# Samba with the dual-head patch from IDEALX. If not using this patch
# just use the same server for slaveLDAP and masterLDAP.
# Those two servers declarations can also be used when you have 
# . one master LDAP server where all writing operations must be done
# . one slave LDAP server where all reading operations must be done
#   (typically a replication directory)

# Slave LDAP server
# Ex: slaveLDAP=127.0.0.1
# If not defined, parameter is set to \"127.0.0.1\"
slaveLDAP=\"$ldap_slave_server\"

# Slave LDAP port
# If not defined, parameter is set to \"389\"
slavePort=\"$ldap_slave_port\"

# Master LDAP server: needed for write operations
# Ex: masterLDAP=127.0.0.1
# If not defined, parameter is set to \"127.0.0.1\"
masterLDAP=\"$ldapmasterserver\"

# Master LDAP port
# If not defined, parameter is set to \"389\"
masterPort=\"$ldapmasterport\"

# Use TLS for LDAP
# If set to 1, this option will use start_tls for connection
# (you should also used the port 389)
# If not defined, parameter is set to \"1\"
ldapTLS=\"$ldaptls\"

# How to verify the server's certificate (none, optional or require)
# see \"man Net::LDAP\" in start_tls section for more details
verify=\"$cert_verify\"

# CA certificate
# see \"man Net::LDAP\" in start_tls section for more details
cafile=\"$cert_cafile\"

# certificate to use to connect to the ldap server
# see \"man Net::LDAP\" in start_tls section for more details
clientcert=\"$cert_clientcert\"

# key certificate to use to connect to the ldap server
# see \"man Net::LDAP\" in start_tls section for more details
clientkey=\"$cert_clientkey\"

# LDAP Suffix
# Ex: suffix=dc=IDEALX,dc=ORG
suffix=\"$ldap_suffix\"

# Where are stored Users
# Ex: usersdn=\"ou=Users,dc=IDEALX,dc=ORG\"
# Warning: if 'suffix' is not set here, you must set the full dn for usersdn
usersdn=\"ou=$ldap_user_suffix,\${suffix}\"

# Where are stored Computers
# Ex: computersdn=\"ou=Computers,dc=IDEALX,dc=ORG\"
# Warning: if 'suffix' is not set here, you must set the full dn for computersdn
computersdn=\"ou=$ldap_machine_suffix,\${suffix}\"

# Where are stored Groups
# Ex: groupsdn=\"ou=Groups,dc=IDEALX,dc=ORG\"
# Warning: if 'suffix' is not set here, you must set the full dn for groupsdn
groupsdn=\"ou=$ldap_group_suffix,\${suffix}\"

# Where are stored Idmap entries (used if samba is a domain member server)
# Ex: groupsdn=\"ou=Idmap,dc=IDEALX,dc=ORG\"
# Warning: if 'suffix' is not set here, you must set the full dn for idmapdn
idmapdn=\"$ldap_idmap_suffix,\${suffix}\"

# Where to store next uidNumber and gidNumber available for new users and groups
# If not defined, entries are stored in sambaDomainName object.
# Ex: sambaUnixIdPooldn=\"sambaDomainName=\${sambaDomain},\${suffix}\"
# Ex: sambaUnixIdPooldn=\"cn=NextFreeUnixId,\${suffix}\"
sambaUnixIdPooldn=\"$sambaUnixIdPooldn,\${suffix}\"

# Default scope Used
scope=\"sub\"

# Unix password encryption (CRYPT, MD5, SMD5, SSHA, SHA, CLEARTEXT)
hash_encrypt=\"$cryp_algo\"

# if hash_encrypt is set to CRYPT, you may set a salt format.
# default is \"\%s\", but many systems will generate MD5 hashed
# passwords if you use \"\$1\$\%\.8s\". This parameter is optional!
crypt_salt_format=\"$crypt_salt_format\"

##############################################################################
# 
# Unix Accounts Configuration
# 
##############################################################################

# Login defs
# Default Login Shell
# Ex: userLoginShell=\"/bin/bash\"
userLoginShell=\"$userLoginShell\"

# Home directory
# Ex: userHome=\"/home/\%U\"
userHome=\"$userHome\"

# Default mode used for user homeDirectory
userHomeDirectoryMode=\"$userHomeDirectoryMode\"

# Gecos
userGecos=\"System User\"

# Default User (POSIX and Samba) GID
defaultUserGid=\"$default_user_gidnumber\"

# Default Computer (Samba) GID
defaultComputerGid=\"$default_computer_gidnumber\"

# Skel dir
skeletonDir=\"$skeletonDir\"

# Default password validation time (time in days) Comment the next line if
# you don't want password to be enable for defaultMaxPasswordAge days (be
# careful to the sambaPwdMustChange attribute's value)
defaultMaxPasswordAge=\"$defaultMaxPasswordAge\"

##############################################################################
#
# SAMBA Configuration
#
##############################################################################

# The UNC path to home drives location (\%U username substitution)
# Just set it to a null string if you want to use the smb.conf 'logon home'
# directive and/or disable roaming profiles
# Ex: userSmbHome=\"\\\\PDC-SMB3\\%U\"
userSmbHome=\"$logonhome\"

# The UNC path to profiles locations (\%U username substitution)
# Just set it to a null string if you want to use the smb.conf 'logon path'
# directive and/or disable roaming profiles
# Ex: userProfile=\"\\\\PDC-SMB3\\profiles\\\%U\"
userProfile=\"$logonpath\"

# The default Home Drive Letter mapping
# (will be automatically mapped at logon time if home directory exist)
# Ex: userHomeDrive=\"H:\"
userHomeDrive=\"$logondrive\"

# The default user netlogon script name (\%U username substitution)
# if not used, will be automatically username.cmd
# make sure script file is edited under dos
# Ex: userScript=\"startup.cmd\" # make sure script file is edited under dos
userScript=\"$userScript\"

# Domain appended to the users \"mail\"-attribute
# when smbldap-useradd -M is used
# Ex: mailDomain=\"idealx.com\"
mailDomain=\"$mailDomain\"

##############################################################################
#
# SMBLDAP-TOOLS Configuration (default are ok for a RedHat)
#
##############################################################################

# Allows not to use smbpasswd (if with_smbpasswd == 0 in smbldap_conf.pm) but
# prefer Crypt::SmbHash library
with_smbpasswd=\"0\"
smbpasswd=\"/usr/bin/smbpasswd\"

# Allows not to use slappasswd (if with_slappasswd == 0 in smbldap_conf.pm)
# but prefer Crypt:: libraries
with_slappasswd=\"0\"
slappasswd=\"/usr/sbin/slappasswd\"

# comment out the following line to get rid of the default banner
# no_banner=\"1\"
";

my $template_smbldap_bind="
############################
# Credential Configuration #
############################
# Notes: you can specify two differents configuration if you use a
# master ldap for writing access and a slave ldap server for reading access
# By default, we will use the same DN (so it will work for standard Samba
# release)
slaveDN=\"$ldap_master_admin_dn\"
slavePw=\"$ldap_master_bind_password\"
masterDN=\"$ldap_slave_admin_dn\"
masterPw=\"$ldap_slave_bind_password\"
";

print "backup old configuration files:\n";
print "  $smbldap_conf->$smbldap_conf.old\n";
print "  $smbldap_bind_conf->$smbldap_bind_conf.old\n";
rename "$smbldap_conf","$smbldap_conf.old";
rename "$smbldap_bind_conf","$smbldap_bind_conf.old";

print "writing new configuration file:\n";
open (SMBLDAP,'>',"$smbldap_conf") || die "Unable to open $smbldap_conf for writing !\n";
print SMBLDAP "$template_smbldap";
close(SMBLDAP);
print "  $smbldap_conf done.\n";
my $mode=0644;
chmod $mode,"$smbldap_conf","$smbldap_conf.old";

open (SMBLDAP_BIND,'>',"$smbldap_bind_conf") || die "Unable to open $smbldap_bind_conf for writing !\n";
print SMBLDAP_BIND "$template_smbldap_bind";
close(SMBLDAP_BIND);
print "  $smbldap_bind_conf done.\n";
$mode=0600;
chmod $mode,"$smbldap_bind_conf","$smbldap_bind_conf.old";



