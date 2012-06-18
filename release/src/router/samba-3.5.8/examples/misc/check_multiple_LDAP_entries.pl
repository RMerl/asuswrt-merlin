#!/usr/bin/perl -w
# Guenther Deschner <gd@samba.org>
#
# check for multiple LDAP entries

use strict;

use Net::LDAP;
use Getopt::Std;

my %opts;

if (!@ARGV) {
	print "usage: $0 -h host -b base -D admindn -w password [-l]\n";
	print "\tperforms checks for multiple sid, uid and gid-entries on your LDAP server\n";
	print "\t-l adds additional checks against the local /etc/passwd and /etc/group file\n";
	exit 1;
}

getopts('b:h:D:w:l', \%opts);

my $host =   $opts{h}	|| "localhost";
my $suffix = $opts{b}	|| die "please set base with -b";
my $binddn = $opts{D}	|| die "please set basedn with -D";
my $bindpw = $opts{w}	|| die "please set password with -w";
my $check_local_files = $opts{l} || 0;

########################


my ($ldap, $res);
my (%passwd_h, %group_h);
my $bad_uids = 0;
my $bad_gids = 0;
my $bad_sids = 0;
my $ret = 0;

if ($check_local_files) {
	my @uids = `cut -d ':' -f 3 /etc/passwd`;
	my @gids = `cut -d ':' -f 3 /etc/group`;
 
	foreach my $uid (@uids) {
		chomp($uid);
		$passwd_h{$uid} = $uid;
	}

	foreach my $gid (@gids) {
		chomp($gid);
		$group_h{$gid} = $gid;
	}
}

########
# bind #
########

$ldap = Net::LDAP->new($host, version => '3');

$res = $ldap->bind( $binddn, password => $bindpw);
$res->code && die "failed to bind: ", $res->error;



###########################
# check for double sids   #
###########################

print "\ntesting for multiple sambaSids\n";

$res = $ldap->search( 
	base => $suffix, 
	filter => "(objectclass=sambaSamAccount)");

$res->code && die "failed to search: ", $res->error;

foreach my $entry ($res->all_entries) {

	my $sid = $entry->get_value('sambaSid');

	my $local_res = $ldap->search( 
		base => $suffix, 
		filter => "(&(objectclass=sambaSamAccount)(sambaSid=$sid))");
	
	$local_res->code && die "failed to search: ", $local_res->error;
	if ($local_res->count > 1) {
		print "A SambaSamAccount with sambaSid [$sid] must exactly exist once\n";
		print "You have ", $local_res->count, " entries:\n";
		foreach my $loc_entry ($local_res->all_entries) {
			printf "\t%s\n", $loc_entry->dn;
		}
		++$bad_sids;
	}
}

if ($bad_sids) {
	$ret = -1;
	print "You have $bad_sids bad sambaSids in your system. You might need to repair them\n";
} else {
	print "No multiple sambaSids found in your system\n";
}

print "-" x 80, "\n";

###########################
# check for double groups #
###########################

print "\ntesting for multiple gidNumbers\n";

$res = $ldap->search( 
	base => $suffix, 
	filter => "(objectclass=posixGroup)");

$res->code && die "failed to search: ", $res->error;

foreach my $entry ($res->all_entries) {

	my $gid = $entry->get_value('gidNumber');
	my $dn  = $entry->dn;

	my $local_res = $ldap->search( 
		base => $suffix, 
		filter => "(&(objectclass=posixGroup)(gidNumber=$gid))");
	
	$local_res->code && die "failed to search: ", $local_res->error;
	if ($local_res->count > 1) {
		print "A PosixGroup with gidNumber [$gid] must exactly exist once\n";
		print "You have ", $local_res->count, " entries:\n";
		foreach my $loc_entry ($local_res->all_entries) {
			printf "\t%s\n", $loc_entry->dn;
		}
		++$bad_gids;
		next;
	}

	if ($check_local_files && exists $group_h{$gid}) {
		print "Warning: There is a group in /etc/group that has gidNumber [$gid] as well\n";
		print "This entry may conflict with $dn\n";
		++$bad_gids;
	}
}

if ($bad_gids) {
	$ret = -1;
	print "You have $bad_gids bad gidNumbers in your system. You might need to repair them\n";
} else {
	print "No multiple gidNumbers found in your system\n";
}

print "-" x 80, "\n";


###########################
# check for double users  #
###########################

print "\ntesting for multiple uidNumbers\n";

$res = $ldap->search( 
	base => $suffix, 
	filter => "(objectclass=posixAccount)");

$res->code && die "failed to search: ", $res->error;


foreach my $entry ($res->all_entries) {

	my $uid = $entry->get_value('uidNumber');
	my $dn  = $entry->dn;

	my $local_res = $ldap->search( 
		base => $suffix, 
		filter => "(&(objectclass=posixAccount)(uidNumber=$uid))");
	
	$local_res->code && die "failed to search: ", $local_res->error;
	if ($local_res->count > 1) {
		print "A PosixAccount with uidNumber [$uid] must exactly exist once\n";
		print "You have ", $local_res->count, " entries:\n";
		foreach my $loc_entry ($local_res->all_entries) {
			printf "\t%s\n", $loc_entry->dn;
		}
		++$bad_uids;
		next;
	}
	if ($check_local_files && exists $passwd_h{$uid}) {
		print "Warning: There is a user in /etc/passwd that has uidNumber [$uid] as well\n";
		print "This entry may conflict with $dn\n";
		++$bad_uids;
	}
}

if ($bad_uids) {
	$ret = -1;
	print "You have $bad_uids bad uidNumbers in your system. You might need to repair them\n";
} else {
	print "No multiple uidNumbers found in your system\n";
}

$ldap->unbind;

exit $ret;
