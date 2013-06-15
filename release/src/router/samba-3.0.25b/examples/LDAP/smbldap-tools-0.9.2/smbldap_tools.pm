#!/usr/bin/perl -w
use strict;
package smbldap_tools;
use Net::LDAP;
use Crypt::SmbHash;
use Unicode::MapUTF8 qw(to_utf8 from_utf8);


# $Id: smbldap_tools.pm,v 1.65 2006/01/02 17:01:19 jtournier Exp $
#
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


# ugly funcs using global variables and spawning openldap clients

my $smbldap_conf;
if (-e "/etc/smbldap-tools/smbldap.conf") {
	$smbldap_conf="/etc/smbldap-tools/smbldap.conf";
} else {
	$smbldap_conf="/etc/opt/IDEALX/smbldap-tools/smbldap.conf";
}

my $smbldap_bind_conf;
if (-e "/etc/smbldap-tools/smbldap_bind.conf") {
	$smbldap_bind_conf="/etc/smbldap-tools/smbldap_bind.conf";
} else {
	$smbldap_bind_conf="/etc/opt/IDEALX/smbldap-tools/smbldap_bind.conf";
}
my $samba_conf;
if (-e "/etc/samba/smb.conf") {
	$samba_conf="/etc/samba/smb.conf";
} else {
	$samba_conf="/usr/local/samba/lib/smb.conf";
}

use vars       qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS);
use Exporter;
$VERSION = 1.00;

@ISA = qw(Exporter);
use vars qw(%config $ldap);

@EXPORT = qw(
	     get_user_dn
	     get_group_dn
	     is_group_member
	     is_samba_user
	     is_unix_user
	     is_nonldap_unix_user
	     is_user_valid
	     does_sid_exist
	     get_dn_from_line
	     add_posix_machine
	     add_samba_machine
	     add_samba_machine_smbpasswd
	     group_add_user
	     add_grouplist_user
	     disable_user
	     delete_user
	     group_add
	     group_del
	     get_homedir
	     read_user
	     read_user_entry
	     read_group
	     read_group_entry
	     read_group_entry_gid
	     find_groups_of
	     parse_group
	     group_remove_member
	     group_get_members
	     do_ldapadd
	     do_ldapmodify
	     get_user_dn2
	     connect_ldap_master
	     connect_ldap_slave
	     group_type_by_name
	     subst_configvar
	     read_config
	     read_parameter
	     subst_user
	     split_arg_comma
	     list_union
	     list_minus
	     get_next_id
	     print_banner
	     getDomainName
	     getLocalSID
	     utf8Encode
	     utf8Decode
	     %config
	    );

sub print_banner
  {
       print STDERR "(c) Jerome Tournier - IDEALX 2004 (http://www.idealx.com)- Licensed under the GPL\n"
         unless $config{no_banner};
  }

sub read_parameter
  {
	my $line=shift;
	## check for a param = value
	if ($_=~/=/) {
	  my ($param,$val);
	  if ($_=~/\s*.*?\s*=\s*".*"/) {
		($param,$val) = /\s*(.*?)\s*=\s*"(.*)"/;
	  } elsif ($_=~/\s*.*?\s*=\s*'.*'/) {
		($param,$val) = /\s*(.*?)\s*=\s*'(.*)'/;
	  } else {
		($param,$val) = /\s*(.*?)\s*=\s*(.*)/;
	  }
	  return ($param,$val);
	}
  }

sub subst_configvar
  {
	my $value = shift;
	my $vars = shift;

	$value =~ s/\$\{([^}]+)\}/$vars->{$1} ? $vars->{$1} : $1/eg;
	return $value;
  }

sub read_conf
  {
	my %conf;
	open (CONFIGFILE, "$smbldap_conf") || die "Unable to open $smbldap_conf for reading !\n";
	while (<CONFIGFILE>) {
	  chomp($_);
	  ## throw away comments
	  next if ( /^\s*#/ || /^\s*$/ || /^\s*\;/);
	  ## check for a param = value
	  my ($parameter,$value)=read_parameter($_);
	  $value = &subst_configvar($value, \%conf);
	  $conf{$parameter}=$value;
	}
	close (CONFIGFILE);

	if ($< == 0) {
	  open (CONFIGFILE, "$smbldap_bind_conf") || die "Unable to open $smbldap_bind_conf for reading !\n";
	  while (<CONFIGFILE>) {
		chomp($_);
		## throw away comments
		next if ( /^\s*#/ || /^\s*$/ || /^\s*\;/);
		## check for a param = value
		my ($parameter,$value)=read_parameter($_);
		$value = &subst_configvar($value, \%conf);
		$conf{$parameter}=$value;
	  }
	  close (CONFIGFILE);
	} else {
	  $conf{slaveDN}=$conf{slavePw}=$conf{masterDN}=$conf{masterPw}="";
	}
       # automatically find SID
       if (not $conf{SID}) {
         $conf{SID} = getLocalSID() ||
           die "Unable to determine domain SID: please edit your smbldap.conf,
  or start your samba server for a few minutes to allow for SID generation to proceed\n";
       }
	return(%conf);
  }

sub read_smbconf
  {
    my %conf;
    my $smbconf="$samba_conf";
    open (CONFIGFILE, "$smbconf") || die "Unable to open $smbconf for reading !\n";
    my $global=0;
    my $prevline="";
    while (<CONFIGFILE>) {
     chomp;
     if (/^(.*)\\$/) {
	$prevline.=$1;
	next;
     }
     $_=$prevline.$_;
     $prevline="";
      if (/^\[global\]/) {
	$global=1;
      }
      if ($global == 1) {
	if (/^\[/ and !/\[global\]/) {
	  $global=0;
	} else {
  	  ## throw away comments
  	  #next if ( ! /workgroup/i );
	  next if ( /^\s*#/ || /^\s*$/ || /^\s*\;/ || /\[/);
	  ## check for a param = value
	  my ($parameter,$value)=read_parameter($_);
	  $value = &subst_configvar($value, \%conf);
	  $conf{$parameter}=$value;
	}
      }
    }
    close (CONFIGFILE);
    return(%conf);
  }
my %smbconf=read_smbconf();

sub getLocalSID {
  my $string = `LANG= PATH=/opt/IDEALX/bin:/usr/local/bin:/usr/bin:/bin net getlocalsid 2>/dev/null`;
  my ($domain,$sid)=($string =~ m/^SID for domain (\S+) is: (\S+)$/ );

  return $sid;
}

# let's read the configurations file...
%config=read_conf();

sub get_parameter {
  # this function return the value for a parameter. The name of the parameter can be either this
  # defined in smb.conf or smbldap.conf
  my $parameter_smb=shift;
  my $parameter_smbldap=shift;
  if (defined $config{$parameter_smbldap} and $config{$parameter_smbldap} ne "") {
	return $config{$parameter_smbldap};
  } elsif (defined $smbconf{$parameter_smb} and $smbconf{$parameter_smb} ne "") {
        return $smbconf{$parameter_smb};
  } else {
	#print "could not find parameter's value (parameter given: $parameter_smbldap or $parameter_smb) !!\n";
	undef $smbconf{$parameter_smb};
  }
  
}

$config{sambaDomain}=get_parameter("workgroup","sambaDomain");
$config{suffix}=get_parameter("ldap suffix","suffix");
$config{usersdn}=get_parameter("ldap user suffix","usersdn");
if ($config{usersdn} !~ m/,/ ) {$config{usersdn}=$config{usersdn}.",".$config{suffix};}
$config{groupsdn}=get_parameter("ldap group suffix","groupsdn");
if ($config{groupsdn} !~ m/,/ ) {$config{groupsdn}=$config{groupsdn}.",".$config{suffix};}
$config{computersdn}=get_parameter("ldap machine suffix","computersdn");
if ($config{computersdn} !~ m/,/ ) {$config{computersdn}=$config{computersdn}.",".$config{suffix};}
$config{idmapdn}=get_parameter("ldap idmap suffix","idmapdn");
if (defined $config{idmapdn}) {
	if ($config{idmapdn} !~ m/,/ ) {$config{idmapdn}=$config{idmapdn}.",".$config{suffix};}
}

# next uidNumber and gidNumber available are stored in sambaDomainName object
if (!defined $config{sambaUnixIdPooldn}) {
	$config{sambaUnixIdPooldn}="sambaDomainName=$config{sambaDomain},$config{suffix}";
}
if (!defined $config{masterLDAP}) {
	$config{masterLDAP}="127.0.0.1";
}
if (!defined $config{masterPort}) {
	$config{masterPort}="389";
}
if (!defined $config{slaveLDAP}) {
	$config{slaveLDAP}="127.0.0.1";
}
if (!defined $config{slavePort}) {
	$config{slavePort}="389";
}
if (!defined $config{ldapTLS}) {
	$config{ldapTLS}="0";
}

sub connect_ldap_master
  {
	# bind to a directory with dn and password
	my $ldap_master = Net::LDAP->new(
									 "$config{masterLDAP}",
									 port => "$config{masterPort}",
									 version => 3,
									 timeout => 60,
									 # debug => 0xffff,
									)
	  or die "erreur LDAP: Can't contact master ldap server ($@)";
	if ($config{ldapTLS} == 1) {
	  $ldap_master->start_tls(
							  verify => "$config{verify}",
							  clientcert => "$config{clientcert}",
							  clientkey => "$config{clientkey}",
							  cafile => "$config{cafile}"
							 );
	}
	$ldap_master->bind ( "$config{masterDN}",
						 password => "$config{masterPw}"
					   );
	$ldap=$ldap_master;
	return($ldap_master);
  }

sub connect_ldap_slave
  {
	# bind to a directory with dn and password
	my $conf_cert;
	my $ldap_slave = Net::LDAP->new(
								 "$config{slaveLDAP}",
								 port => "$config{slavePort}",
								 version => 3,
								 timeout => 60,
								 # debug => 0xffff,
								)
	  or warn "erreur LDAP: Can't contact slave ldap server ($@)\n=>trying to contact the master server\n";
	if (!$ldap_slave) {
	  # connection to the slave failed: trying to contact the master ...
	  $ldap_slave = Net::LDAP->new(
								   "$config{masterLDAP}",
								   port => "$config{masterPort}",
								   version => 3,
								   timeout => 60,
								   # debug => 0xffff,
								  )
		or die "erreur LDAP: Can't contact master ldap server ($@)\n";
	}
	if ($ldap_slave) {
	  if ($config{ldapTLS} == 1) {
		$ldap_slave->start_tls(
							   verify => "$config{verify}",
							   clientcert => "$config{clientcert}",
							   clientkey => "$config{clientkey}",
							   cafile => "$config{cafile}"
							  );
	  }
	  $ldap_slave->bind ( "$config{masterDN}",
						  password => "$config{masterPw}"
						);
	  $ldap=$ldap_slave;
	  return($ldap_slave);
	}
  }

sub get_user_dn
  {
    my $user = shift;
    my $dn='';
    my  $mesg = $ldap->search (    base   => $config{suffix},
										 scope => $config{scope},
										 filter => "(&(objectclass=posixAccount)(uid=$user))"
									);
    $mesg->code && die $mesg->error;
    foreach my $entry ($mesg->all_entries) {
	  $dn= $entry->dn;
	}
    chomp($dn);
    if ($dn eq '') {
	  return undef;
    }
    $dn="dn: ".$dn;
    return $dn;
  }


sub get_user_dn2
  {
    my $user = shift;
    my $dn='';
    my  $mesg = $ldap->search (    base   => $config{suffix},
										 scope => $config{scope},
										 filter => "(&(objectclass=posixAccount)(uid=$user))"
									);
    $mesg->code && warn "failed to perform search; ", $mesg->error;

    foreach my $entry ($mesg->all_entries) {
	  $dn= $entry->dn;
    }
    chomp($dn);
    if ($dn eq '') {
	  return (1,undef);
    }
    $dn="dn: ".$dn;
    return (1,$dn);
  }


sub get_group_dn
  {
	my $group = shift;
	my $dn='';
	my $filter;
	if ($group =~ /^\d+$/) {
	  $filter="(&(objectclass=posixGroup)(|(cn=$group)(gidNumber=$group)))";
	} else {
	  $filter="(&(objectclass=posixGroup)(cn=$group))";
	}
	my  $mesg = $ldap->search (    base   => $config{groupsdn},
										 scope => $config{scope},
										 filter => $filter
									);
	$mesg->code && die $mesg->error;
	foreach my $entry ($mesg->all_entries) {
	  $dn= $entry->dn;
	}
	chomp($dn);
	if ($dn eq '') {
	  return undef;
	}
	$dn="dn: ".$dn;
	return $dn;
  }

# return (success, dn)
# bool = is_samba_user($username)
sub is_samba_user
  {
	my $user = shift;
	my $mesg = $ldap->search (    base   => $config{suffix},
										scope => $config{scope},
										filter => "(&(objectClass=sambaSamAccount)(uid=$user))"
								   );
	$mesg->code && die $mesg->error;
	return ($mesg->count ne 0);
  }

sub is_unix_user
  {
	my $user = shift;
	my $mesg = $ldap->search (    base   => $config{suffix},
										scope => $config{scope},
										filter => "(&(objectClass=posixAccount)(uid=$user))"
								   );
	$mesg->code && die $mesg->error;
	return ($mesg->count ne 0);
  }

sub is_nonldap_unix_user
  {
	my $user = shift;
	my $uid = getpwnam($user);

	if ($uid) {
		return 1;
	} else {
		return 0;
	}
}


sub is_group_member
  {
	my $dn_group = shift;
	my $user = shift;
	my $mesg = $ldap->search (   base   => $dn_group,
									   scope => 'base',
									   filter => "(&(memberUid=$user))"
								   );
	$mesg->code && die $mesg->error;
	return ($mesg->count ne 0);
  }

# all entries = does_sid_exist($sid,$config{scope})
sub does_sid_exist
  {
	my $sid = shift;
	my $dn_group=shift;
	my $mesg = $ldap->search (    base   => $dn_group,
										scope => $config{scope},
										filter => "(sambaSID=$sid)"
										#filter => "(&(objectClass=sambaSAMAccount|objectClass=sambaGroupMapping)(sambaSID=$sid))"
								   );
	$mesg->code && die $mesg->error;
	return ($mesg);
  }

# try to bind with user dn and password to validate current password
sub is_user_valid
  {
	my ($user, $dn, $pass) = @_;
       my $userLdap = Net::LDAP->new(
                                                                "$config{slaveLDAP}",
                                                                port => "$config{slavePort}",
                                                                version => 3,
                                                                timeout => 60
                                                                )
         or warn "erreur LDAP: Can't contact slave ldap server ($@)\n=>trying to contact the master server\n";
       if (!$userLdap) {
         # connection to the slave failed: trying to contact the master ...
         $userLdap = Net::LDAP->new(
                                                                "$config{masterLDAP}",
                                                                port => "$config{masterPort}",
                                                                version => 3,
                                                                timeout => 60
                                                                )
               or die "erreur LDAP: Can't contact master ldap server ($@)\n";
       }
       if ($userLdap) {
         if ($config{ldapTLS} == 1) {
               $userLdap->start_tls(
                                                          verify => "$config{verify}",
                                                          clientcert => "$config{clientcert}",
                                                          clientkey => "$config{clientkey}",
                                                          cafile => "$config{cafile}"
                                                         );
         }
         my $mesg= $userLdap->bind (dn => $dn, password => $pass );
         if ($mesg->code eq 0) {
           $userLdap->unbind;
           return 1;
	  } else {
           if ($userLdap->bind()) {
             $userLdap->unbind;
             return 0;
           } else {
             print ("The LDAP directory is not available.\n Check the server, cables ...");
             $userLdap->unbind;
             return 0;
           }
           die "Problem : contact your administrator";
	  }
	}
  }


# dn = get_dn_from_line ($dn_line)
# helper to get "a=b,c=d" from "dn: a=b,c=d"
sub get_dn_from_line
  {
	my $dn = shift;
	$dn =~ s/^dn: //;
	return $dn;
  }


# success = add_posix_machine($user, $uid, $gid)
sub add_posix_machine
  {
	my ($user,$uid,$gid,$wait) = @_;
	if (!defined $wait) {
		$wait=0;
	}
	# bind to a directory with dn and password
	my $add = $ldap->add ( "uid=$user,$config{computersdn}",
								  attr => [
										   'objectclass' => ['top', 'person', 'organizationalPerson', 'inetOrgPerson', 'posixAccount'],
										   'cn'   => "$user",
										   'sn'   => "$user",
										   'uid'   => "$user",
										   'uidNumber'   => "$uid",
										   'gidNumber'   => "$gid",
										   'homeDirectory'   => '/dev/null',
										   'loginShell'   => '/bin/false',
										   'description'   => 'Computer',
                                                                                  'gecos'   => 'Computer',
										  ]
								);
	
	$add->code && warn "failed to add entry: ", $add->error ;
	sleep($wait);
	return 1;
  }


# success = add_samba_machine_smbpasswd($computername)
sub add_samba_machine_smbpasswd
  {
    my $user = shift;
    system "smbpasswd -a -m $user";
    return 1;
  }

sub add_samba_machine
  {
	my ($user, $uid) = @_;
	my $sambaSID = 2 * $uid + 1000;
	my $name = $user;
	$name =~ s/.$//s;

	my ($lmpassword,$ntpassword) = ntlmgen $name;	
	my $modify = $ldap->modify ( "uid=$user,$config{computersdn}",
										changes => [
													replace => [objectClass => ['inetOrgPerson', 'posixAccount', 'sambaSAMAccount']],
													add => [sambaPwdLastSet => '0'],
													add => [sambaLogonTime => '0'],
													add => [sambaLogoffTime => '2147483647'],
													add => [sambaKickoffTime => '2147483647'],
													add => [sambaPwdCanChange => '0'],
													add => [sambaPwdMustChange => '0'],
													add => [sambaAcctFlags => '[W          ]'],
													add => [sambaLMPassword => "$lmpassword"],
													add => [sambaNTPassword => "$ntpassword"],
													add => [sambaSID => "$config{SID}-$sambaSID"],
													add => [sambaPrimaryGroupSID => "$config{SID}-0"]
												   ]
									  );
	
	$modify->code && die "failed to add entry: ", $modify->error ;

	return 1;
  }

sub group_add_user
  {
	my ($group, $userid) = @_;
	my $members='';
	my $dn_line = get_group_dn($group);
	if (!defined(get_group_dn($group))) {
	  print "$0: group \"$group\" doesn't exist\n";
	  exit (6); 
	}
	if (!defined($dn_line)) {
	  return 1;
	}
	my $dn = get_dn_from_line("$dn_line");
	# on look if the user is already present in the group
	my $is_member=is_group_member($dn,$userid);
	if ($is_member == 1) {
	  print "User \"$userid\" already member of the group \"$group\".\n";
	} else {
	  # bind to a directory with dn and password
	  # It does not matter if the user already exist, Net::LDAP will add the user
	  # if he does not exist, and ignore him if his already in the directory.
	  my $modify = $ldap->modify ( "$dn",
										  changes => [
													  add => [memberUid => $userid]
													 ]
										);
	  $modify->code && die "failed to modify entry: ", $modify->error ;
	}
  }

sub group_del
  {
	my $group_dn=shift;
	# bind to a directory with dn and password
	my $modify = $ldap->delete ($group_dn);
	$modify->code && die "failed to delete group : ", $modify->error ;
  }

sub add_grouplist_user
  {
	my ($grouplist, $user) = @_;
	my @array = split(/,/, $grouplist);
	foreach my $group (@array) {
	  group_add_user($group, $user);
	}
  }

sub disable_user
  {
	my $user = shift;
	my $dn_line;
	my $dn = get_dn_from_line($dn_line);
	
	if (!defined($dn_line = get_user_dn($user))) {
	  print "$0: user $user doesn't exist\n";
	  exit (10);
	}
	my $modify = $ldap->modify ( "$dn",
										changes => [
													replace => [userPassword => '{crypt}!x']
												   ]
									  );
	$modify->code && die "failed to modify entry: ", $modify->error ;

	if (is_samba_user($user)) {
	  my $modify = $ldap->modify ( "$dn",
										  changes => [
													  replace => [sambaAcctFlags => '[D       ]']
													 ]
										);
	  $modify->code && die "failed to modify entry: ", $modify->error ;
	}
  }

# delete_user($user)
sub delete_user
  {
	my $user = shift;
	my $dn_line;

	if (!defined($dn_line = get_user_dn($user))) {
	  print "$0: user $user doesn't exist\n";
	  exit (10);
	}

	my $dn = get_dn_from_line($dn_line);
	my $modify = $ldap->delete($dn);
  }

# $gid = group_add($groupname, $group_gid, $force_using_existing_gid)
sub group_add
  {
	my ($gname, $gid, $force) = @_;
	my $nscd_status = system "/etc/init.d/nscd status >/dev/null 2>&1";
	if ($nscd_status == 0) {
	  system "/etc/init.d/nscd stop > /dev/null 2>&1";
	}
	if (!defined($gid)) {
	  #while (defined(getgrgid($config{GID_START}))) {
	  #	$config{GID_START}++;
	  #}
	  #$gid = $config{GID_START};
	  $gid=get_next_id($config{groupsdn},"gidNumber");
	} else {
	  if (!defined($force)) {
		if (defined(getgrgid($gid))) {
		  return undef;
		}
	  }
	}
	if ($nscd_status == 0) {
	  system "/etc/init.d/nscd start > /dev/null 2>&1";
	}
	my $modify = $ldap->add ( "cn=$gname,$config{groupsdn}",
									 attrs => [
											   objectClass => [ 'top', 'posixGroup' ],
											   cn => "$gname",
											   gidNumber => "$gid"
											  ]
								   );
	
	$modify->code && die "failed to add entry: ", $modify->error ;
	return $gid;
  }

# $homedir = get_homedir ($user)
sub get_homedir
  {
	my $user = shift;
	my $homeDir='';
	my $entry;
	my  $mesg = $ldap->search (
									 base   =>$config{usersdn},
									 scope => $config{scope},
									 filter => "(&(objectclass=posixAccount)(uid=$user))"
									);
	$mesg->code && die $mesg->error;

	my $nb=$mesg->count;
	if ($nb > 1) {
	  print "Aborting: there are $nb existing user named $user\n";
	  foreach $entry ($mesg->all_entries) {
		my $dn=$entry->dn;
		print "  $dn\n";
	  }
	  exit (4);
	} else {
	  $entry = $mesg->shift_entry();
	  $homeDir= $entry->get_value("homeDirectory");
	}

	chomp $homeDir;
	if ($homeDir eq '') {
	  return undef;
	}
	return $homeDir;
  }

# search for an user
sub read_user
  {
	my $user = shift;
	my $lines ='';
	my $mesg = $ldap->search ( # perform a search
									base   => $config{suffix},
									scope => $config{scope},
									filter => "(&(objectclass=posixAccount)(uid=$user))"
								   );

	$mesg->code && die $mesg->error;
	foreach my $entry ($mesg->all_entries) {
	  $lines.= "dn: " . $entry->dn."\n";
	  foreach my $attr ($entry->attributes) {
		{
		  $lines.= $attr.": ".join(',', $entry->get_value($attr))."\n";
		}
	  }
	}
	chomp $lines;
	if ($lines eq '') {
	  return undef;
	}
	return $lines;
  }

# search for a user
# return the attributes in an array
sub read_user_entry
  {
	my $user = shift;
	my  $mesg = $ldap->search ( # perform a search
									 base   => $config{suffix},
									 scope => $config{scope},
									 filter => "(&(objectclass=posixAccount)(uid=$user))"
									);

	$mesg->code && die $mesg->error;
	my $entry = $mesg->entry();
	return $entry;
  }

# search for a group
sub read_group
  {
	my $user = shift;
	my $lines ='';
	my  $mesg = $ldap->search ( # perform a search
									 base   => $config{groupsdn},
									 scope => $config{scope},
									 filter => "(&(objectclass=posixGroup)(cn=$user))"
									);

	$mesg->code && die $mesg->error;
	foreach my $entry ($mesg->all_entries) {
	  $lines.= "dn: " . $entry->dn."\n";
	  foreach my $attr ($entry->attributes) {
		{
		  $lines.= $attr.": ".join(',', $entry->get_value($attr))."\n";
		}
	  }
	}
	chomp $lines;
	if ($lines eq '') {
	  return undef;
	}
	return $lines;
  }

# find groups of a given user
##### MODIFIE ########
sub find_groups_of {
  my $user = shift;
  my @groups = ();
  my $mesg = $ldap->search ( # perform a search
                                  base   => $config{groupsdn},
                                  scope => $config{scope},
                                  filter => "(&(objectclass=posixGroup)(memberuid=$user))"
                                 );
  $mesg->code && die $mesg->error;

  my $entry;
  while ($entry = $mesg->shift_entry()) {
    push(@groups, scalar($entry->get_value('cn')));
  }
  return (@groups);
}

sub read_group_entry {
  my $group = shift;
  my $entry;
  my %res;
  my  $mesg = $ldap->search ( # perform a search
								   base   => $config{groupsdn},
								   scope => $config{scope},
								   filter => "(&(objectclass=posixGroup)(cn=$group))"
								  );

  $mesg->code && die $mesg->error;
  my $nb=$mesg->count;
  if ($nb > 1) {
    print "Error: $nb groups exist \"cn=$group\"\n";
    foreach $entry ($mesg->all_entries) {
	  my $dn=$entry->dn; print "  $dn\n";
	}
    exit 11;
  } else {
    $entry = $mesg->shift_entry();
  }
  return $entry;
}

sub read_group_entry_gid {
  my $group = shift;
  my %res;
  my  $mesg = $ldap->search ( # perform a search
								   base   => $config{groupsdn},
								   scope => $config{scope},
								   filter => "(&(objectclass=posixGroup)(gidNumber=$group))"
								  );

  $mesg->code && die $mesg->error;
  my $entry = $mesg->shift_entry();
  return $entry;
}

# return the gidnumber for a group given as name or gid
# -1 : bad group name
# -2 : bad gidnumber
sub parse_group
  {
	my $userGidNumber = shift;
	if ($userGidNumber =~ /[^\d]/ ) {
	  my $gname = $userGidNumber;
	  my $gidnum = getgrnam($gname);
	  if ($gidnum !~ /\d+/) {
		return -1;
	  } else {
		$userGidNumber = $gidnum;
	  }
	} elsif (!defined(getgrgid($userGidNumber))) {
	  return -2;
	}
	return $userGidNumber;
  }

# remove $user from $group
sub group_remove_member
  {
	my ($group, $user) = @_;
	my $members='';
	my $grp_line = get_group_dn($group);
	if (!defined($grp_line)) {
	  return 0;
	}
	my $dn = get_dn_from_line($grp_line);
	# we test if the user exist in the group
	my $is_member=is_group_member($dn,$user);
	if ($is_member == 1) {
	  # delete only the user from the group
	  my $modify = $ldap->modify ( "$dn",
										  changes => [
													  delete => [memberUid => ["$user"]]
													 ]
										);
	  $modify->code && die "failed to delete entry: ", $modify->error ;
	}
	return 1;
  }

sub group_get_members
  {
	my ($group) = @_;
	my $members;
	my @resultat;
	my $grp_line = get_group_dn($group);
	if (!defined($grp_line)) {
	  return 0;
	}
	my  $mesg = $ldap->search (
							   base   => $config{groupsdn},
							   scope => $config{scope},
							   filter => "(&(objectclass=posixgroup)(cn=$group))"
							  );
	$mesg->code && die $mesg->error;
	foreach my $entry ($mesg->all_entries) {
	  foreach my $attr ($entry->attributes) {
		if ($attr=~/\bmemberUid\b/) {
		  foreach my $ent ($entry->get_value($attr)) {
			push (@resultat,$ent);
		  }
		}
	  }
	}
	return @resultat;
  }

sub do_ldapmodify
  {
	my $ldif = shift;
	my $FILE = "|$config{ldapmodify} -r >/dev/null";
	open (FILE, $FILE) || die "$!\n";
	print FILE <<EOF;
$ldif
EOF
	;
	close FILE;
	my $rc = $?;
	return $rc;
  }

sub group_type_by_name {
  my $type_name = shift;
  my %groupmap = (
				  'domain' => 2,
				  'local' => 4,
				  'builtin' => 5
				 );
  return $groupmap{$type_name};
}

sub subst_user
  {
	my ($str, $username) = @_;
	$str =~ s/%U/$username/ if ($str);
	return($str);
  }

# all given mails are stored in a table (remove the comma separated)
sub split_arg_comma {
  my $arg = shift;
  my @args;
  if (defined($arg)) {
    if ($arg eq '-') {
      @args = ( );
    } else {
      @args = split(/\s*,\s*/, $arg);
    }
  }
  return (@args);
}

sub list_union {
  my ($list1, $list2) = @_;
  my @res = @$list1;
  foreach my $e (@$list2) {
    if (! grep($_ eq $e, @$list1)) {
      push(@res, $e);
    }
  }
  return @res;
}

sub list_minus {
  my ($list1, $list2) = @_;
  my @res = ();
  foreach my $e (@$list1) {
    if (! grep( $_ eq $e, @$list2 )) {
      push(@res, $e);
    }
  }
  return @res;
}

sub get_next_id($$) {
  my $ldap_base_dn = shift;
  my $attribute = shift;
  my $tries = 0;
  my $found=0;
  my $next_uid_mesg;
  my $nextuid;
  if ($ldap_base_dn =~ m/$config{usersdn}/i) {
	# when adding a new user, we'll check if the uidNumber available is not
	# already used for a computer's account
	$ldap_base_dn=$config{suffix}
  }
  do {
	$next_uid_mesg = $ldap->search(
										  base => $config{sambaUnixIdPooldn},
										  filter => "(objectClass=sambaUnixIdPool)",
										  scope => "base"
										 );
	$next_uid_mesg->code && die "Error looking for next uid";
	if ($next_uid_mesg->count != 1) {
	  die "Could not find base dn, to get next $attribute";
	}
	my $entry = $next_uid_mesg->entry(0);
            
	$nextuid = $entry->get_value($attribute);
	my $modify=$ldap->modify( "$config{sambaUnixIdPooldn}",
									 changes => [
												 replace => [ $attribute => $nextuid + 1 ]
												]
								   );
	$modify->code && die "Error: ", $modify->error;
	# let's check if the id found is really free (in ou=Groups or ou=Users)...
	my $check_uid_mesg = $ldap->search(
											  base => $ldap_base_dn,
											  filter => "($attribute=$nextuid)",
											 );
	$check_uid_mesg->code && die "Cannot confirm $attribute $nextuid is free";
	if ($check_uid_mesg->count == 0) {
	  $found=1;
	  return $nextuid;
	}
	$tries++;
	print "Cannot confirm $attribute $nextuid is free: checking for the next one\n"
  } while ($found != 1);
  die "Could not allocate $attribute!";
}

sub utf8Encode {
  my $arg = shift; 

  return to_utf8(
                                 -string=> $arg,
                                 -charset => 'ISO-8859-1',
                                );
}

sub utf8Decode {
  my $arg = shift;

  return from_utf8(
                                   -string=> $arg,
                                   -charset => 'ISO-8859-1',
                                  );
}   

1;

