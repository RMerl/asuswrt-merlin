#!/usr/bin/perl
#

use strict;

use Getopt::Long;
use Cwd qw(abs_path);

my $opt_help = 0;
my $opt_passwd_path = undef;
my $opt_group_path = undef;
my $opt_action = undef;
my $opt_type = undef;
my $opt_name = undef;
my $opt_member = undef;
my $opt_gid = 65534;# nogroup gid

my $passwdfn = undef;
my $groupfn = undef;
my $memberfn = undef;
my $actionfn = undef;

sub passwd_add($$$$$);
sub passwd_delete($$$$$);
sub group_add($$$$$);
sub group_delete($$$$$);
sub member_add($$$$$);
sub member_delete($$$$$);

sub check_path($$);

my $result = GetOptions(
	'help|h|?'	=> \$opt_help,
	'passwd_path=s'	=> \$opt_passwd_path,
	'group_path=s'	=> \$opt_group_path,
	'action=s'	=> \$opt_action,
	'type=s'	=> \$opt_type,
	'name=s'	=> \$opt_name,
	'member=s'	=> \$opt_member,
	'gid=i'		=> \$opt_gid
);

sub usage($;$)
{
	my ($ret, $msg) = @_;

	print $msg."\n\n" if defined($msg);

	print "usage:

	--help|-h|-?		Show this help.

	--passwd_path <path>	Path of the 'passwd' file.
	--group_path <path>	Path of the 'group' file.

	--type <type>		'passwd', 'group' and 'member' are supported.

	--action <action>	'add' or 'delete'.

	--name <name>		The name of the object.

	--member <member>	The name of the member.

	--gid <gid>		Primary Group ID for new users.
";
	exit($ret);
}

usage(1) if (not $result);

usage(0) if ($opt_help);

if (not defined($opt_action)) {
	usage(1, "missing: --action [add|delete]");
}
if ($opt_action eq "add") {
	$passwdfn = \&passwd_add;
	$groupfn = \&group_add;
	$memberfn = \&member_add;
} elsif ($opt_action eq "delete") {
	$passwdfn = \&passwd_delete;
	$groupfn = \&group_delete;
	$memberfn = \&member_delete;
} else {
	usage(1, "invalid: --action [add|delete]: '$opt_action'");
}

if (not defined($opt_type)) {
	usage(1, "missing: --type [passwd|group|member]");
}
if ($opt_type eq "member" and not defined($opt_member)) {
	usage(1, "missing: --member <member>");
}
my $opt_fullpath_passwd;
my $opt_fullpath_group;
if ($opt_type eq "passwd") {
	$actionfn = $passwdfn;
	$opt_fullpath_passwd = check_path($opt_passwd_path, $opt_type);
} elsif ($opt_type eq "group") {
	$actionfn = $groupfn;
	$opt_fullpath_group = check_path($opt_group_path, $opt_type);
} elsif ($opt_type eq "member") {
	$actionfn = $memberfn;
	$opt_fullpath_passwd = check_path($opt_passwd_path, "passwd");
	$opt_fullpath_group = check_path($opt_group_path, "group");
} else {
	usage(1, "invalid: --type [passwd|group]: '$opt_type'")
}

if (not defined($opt_name)) {
	usage(1, "missing: --name <name>");
}
if ($opt_name eq "") {
	usage(1, "invalid: --name <name>");
}

exit $actionfn->($opt_fullpath_passwd, $opt_member, $opt_fullpath_group, $opt_name, $opt_gid);

sub check_path($$)
{
	my ($path,$type) = @_;

	if (not defined($path)) {
		usage(1, "missing: --$type\_path <path>");
	}
	if ($path eq "" or $path eq "/") {
		usage(1, "invalid: --$type\_path <path>: '$path'");
	}
	my $fullpath = abs_path($path);
	if (not defined($fullpath)) {
		usage(1, "invalid: --$type\_path <path>: '$path'");
	}
	return $fullpath;
}

sub passwd_add_entry($$);

sub passwd_load($)
{
	my ($path) = @_;
	my @lines;
	my $passwd = undef;

	open(PWD, "<$path") or die("Unable to open '$path' for read");
	@lines = <PWD>;
	close(PWD);

	$passwd->{array} = ();
	$passwd->{name} = {};
	$passwd->{uid} = {};
	$passwd->{path} = $path;

	foreach my $line (@lines) {
		passwd_add_entry($passwd, $line);
	}

	return $passwd;
}

sub group_add_entry($$);

sub group_load($)
{
	my ($path) = @_;
	my @lines;
	my $group = undef;

	open(GROUP, "<$path") or die("Unable to open '$path' for read");
	@lines = <GROUP>;
	close(GROUP);

	$group->{array} = ();
	$group->{name} = {};
	$group->{gid} = {};
	$group->{path} = $path;

	foreach my $line (@lines) {
		group_add_entry($group, $line);
	}

	return $group;
}

sub passwd_lookup_name($$)
{
	my ($passwd, $name) = @_;

	return undef unless defined($passwd->{name}{$name});

	return $passwd->{name}{$name};
}

sub group_lookup_name($$)
{
	my ($group, $name) = @_;

	return undef unless defined($group->{name}{$name});

	return $group->{name}{$name};
}

sub passwd_lookup_uid($$)
{
	my ($passwd, $uid) = @_;

	return undef unless defined($passwd->{uid}{$uid});

	return $passwd->{uid}{$uid};
}

sub group_lookup_gid($$)
{
	my ($group, $gid) = @_;

	return undef unless defined($group->{gid}{$gid});

	return $group->{gid}{$gid};
}

sub passwd_get_free_uid($)
{
	my ($passwd) = @_;
	my $uid = 1000;

	while (passwd_lookup_uid($passwd, $uid)) {
		$uid++;
	}

	return $uid;
}

sub group_get_free_gid($)
{
	my ($group) = @_;
	my $gid = 1000;

	while (group_lookup_gid($group, $gid)) {
		$gid++;
	}

	return $gid;
}

sub passwd_add_entry($$)
{
	my ($passwd, $str) = @_;

	chomp $str;
	my @e = split(':', $str);

	push(@{$passwd->{array}}, \@e);
	$passwd->{name}{$e[0]} = \@e;
	$passwd->{uid}{$e[2]} = \@e;
}

sub group_add_entry($$)
{
	my ($group, $str) = @_;

	chomp $str;
	my @e = split(':', $str);

	push(@{$group->{array}}, \@e);
	$group->{name}{$e[0]} = \@e;
	$group->{gid}{$e[2]} = \@e;
}

sub passwd_remove_entry($$)
{
	my ($passwd, $eref) = @_;

	for (my $i = 0; defined($passwd->{array}[$i]); $i++) {
		if ($eref == $passwd->{array}[$i]) {
			$passwd->{array}[$i] = undef;
		}
	}

	delete $passwd->{name}{${$eref}[0]};
	delete $passwd->{uid}{${$eref}[2]};
}

sub group_remove_entry($$)
{
	my ($group, $eref) = @_;

	for (my $i = 0; defined($group->{array}[$i]); $i++) {
		if ($eref == $group->{array}[$i]) {
			$group->{array}[$i] = undef;
		}
	}

	delete $group->{name}{${$eref}[0]};
	delete $group->{gid}{${$eref}[2]};
}

sub group_add_member($$$)
{
	my ($group, $eref, $username) = @_;

	my @members;
	my $str = @$eref[3] || undef;
	if ($str) {
		@members = split(",", $str);
	}

	foreach my $member (@members) {
		if ($member and $member eq $username) {
			die("account[$username] is already member of '@$eref[0]'");
		}
	}

	push(@members, $username);

	my $gwent = @$eref[0].":x:".@$eref[2].":".join(",", @members);

	group_remove_entry($group, $eref);

	group_add_entry($group, $gwent);
}

sub group_delete_member($$$)
{
	my ($group, $eref, $username) = @_;

	my @members = undef;
	my $str = @$eref[3] || undef;
	if ($str) {
		@members = split(",", $str);
	}
	my @new_members;
	my $removed = 0;

	foreach my $member (@members) {
		if ($member and $member ne $username) {
			push(@new_members, $member);
		} else {
			$removed = 1;
		}
	}

	if ($removed != 1) {
		die("account[$username] is not member of '@$eref[0]'");
	}

	my $gwent = @$eref[0].":x:".@$eref[2].":".join(",", @new_members);

	group_remove_entry($group, $eref);

	group_add_entry($group, $gwent);
}

sub passwd_save($)
{
	my ($passwd) = @_;
	my @lines = ();
	my $path = $passwd->{path};
	my $tmppath = $path.$$;

	foreach my $eref (@{$passwd->{array}}) {
		next unless defined($eref);

		my $line = join(':', @{$eref});
		push(@lines, $line);
	}

	open(PWD, ">$tmppath") or die("Unable to open '$tmppath' for write");
	print PWD join("\n", @lines)."\n";
	close(PWD);
	rename($tmppath, $path) or die("Unable to rename $tmppath => $path");
}

sub group_save($)
{
	my ($group) = @_;
	my @lines = ();
	my $path = $group->{path};
	my $tmppath = $path.$$;

	foreach my $eref (@{$group->{array}}) {
		next unless defined($eref);

		my $line = join(':', @{$eref});
		if (scalar(@{$eref}) == 3) {
			$line .= ":";
		}
		push(@lines, $line);
	}

	open(GROUP, ">$tmppath") or die("Unable to open '$tmppath' for write");
	print GROUP join("\n", @lines)."\n";
	close(GROUP);
	rename($tmppath, $path) or die("Unable to rename $tmppath => $path");
}

sub passwd_add($$$$$)
{
	my ($path, $dummy, $dummy2, $name, $gid) = @_;

	#print "passwd_add: '$name' in '$path'\n";

	my $passwd = passwd_load($path);

	my $e = passwd_lookup_name($passwd, $name);
	die("account[$name] already exists in '$path'") if defined($e);

	my $uid = passwd_get_free_uid($passwd);

	my $pwent = $name.":x:".$uid.":".$gid.":".$name." gecos:/nodir:/bin/false";

	passwd_add_entry($passwd, $pwent);

	passwd_save($passwd);

	return 0;
}

sub passwd_delete($$$$$)
{
	my ($path, $dummy, $dummy2, $name, $dummy3) = @_;

	#print "passwd_delete: '$name' in '$path'\n";

	my $passwd = passwd_load($path);

	my $e = passwd_lookup_name($passwd, $name);
	die("account[$name] does not exists in '$path'") unless defined($e);

	passwd_remove_entry($passwd, $e);

	passwd_save($passwd);

	return 0;
}

sub group_add($$$$$)
{
	my ($dummy, $dummy2, $path, $name, $dummy3) = @_;

	#print "group_add: '$name' in '$path'\n";

	my $group = group_load($path);

	my $e = group_lookup_name($group, $name);
	die("group[$name] already exists in '$path'") if defined($e);

	my $gid = group_get_free_gid($group);

	my $gwent = $name.":x:".$gid.":"."";

	group_add_entry($group, $gwent);

	group_save($group);

	#printf("%d\n", $gid);

	return 0;
}

sub group_delete($$$$$)
{
	my ($dummy, $dummy2, $path, $name, $dummy3) = @_;

	#print "group_delete: '$name' in '$path'\n";

	my $group = group_load($path);

	my $e = group_lookup_name($group, $name);
	die("group[$name] does not exists in '$path'") unless defined($e);

	group_remove_entry($group, $e);

	group_save($group);

	return 0;
}

sub member_add($$$$$)
{
	my ($passwd_path, $username, $group_path, $groupname, $dummy) = @_;

	#print "member_add: adding '$username' in '$passwd_path' to '$groupname' in '$group_path'\n";

	my $group = group_load($group_path);

	my $g = group_lookup_name($group, $groupname);
	die("group[$groupname] does not exists in '$group_path'") unless defined($g);

	my $passwd = passwd_load($passwd_path);

	my $u = passwd_lookup_name($passwd, $username);
	die("account[$username] does not exists in '$passwd_path'") unless defined($u);

	group_add_member($group, $g, $username);

	group_save($group);

	return 0;
}

sub member_delete($$$$$)
{
	my ($passwd_path, $username, $group_path, $groupname, $dummy) = @_;

	#print "member_delete: removing '$username' in '$passwd_path' from '$groupname' in '$group_path'\n";

	my $group = group_load($group_path);

	my $g = group_lookup_name($group, $groupname);
	die("group[$groupname] does not exists in '$group_path'") unless defined($g);

	my $passwd = passwd_load($passwd_path);

	my $u = passwd_lookup_name($passwd, $username);
	die("account[$username] does not exists in '$passwd_path'") unless defined($u);

	group_delete_member($group, $g, $username);

	group_save($group);

	return 0;
}
