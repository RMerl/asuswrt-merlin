#!/usr/bin/perl

#
# adduserstogroups.pl
#
#    add single or continuously numbered domain users
#    to a given single group or list of groups
#
# Copyright (C) Michael Adam <obnox@samba.org> 2007
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the Free
# Software Foundation; either version 3 of the License, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
# more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, see <http://www.gnu.org/licenses/>.
#

#
# WARNING: This script is still rather crude.
#

use strict;
use Getopt::Std;

my $net_cmd	= "net";

# defaults:

my $server;
my $num_members	= 1;
my $startmem;			# if empty, don't add numbers to member prefix
my $member_prefix;		# name prefix for member
my $num_groups = 1;
my $startgroup;			# if empty, don't add numbers to group prefix
my $group_prefix;		# name prefix for group
my $path;			# path to rpcclient command
my $net_path	= $net_cmd;
my $creds;

sub usage {
	print "USAGE: $0 [-h] -S server -U user\%pass \\\n"
		. "\t-m member [-s startmem] [-n nummem] \\\n"
		. "\t-g group [-G startgroup] [-N numgroups] \\\n"
		. "\t[-P path]\n";
}

# parse commandline:

my %options = ();
getopts("U:S:m:s:n:g:G:N:P:h", \%options);

if (exists($options{h})) {
	usage();
	exit 0;
}

if (exists($options{g})) {
	$group_prefix = $options{g};
}
else {
	print "ERROR: mandatory argument '-g' missing\n";
	usage();
	exit 1;
}

if (exists($options{U})) {
	$creds = "-U $options{U}";
	if ($creds !~ '%') {
		print "ERROR: you need to specify credentials in the form -U user\%pass\n";
		usage();
		exit 1;
	}
}
else {
	print "ERROR: mandatory argument '-U' missing\n";
	usage();
	exit 1;
}

if (exists($options{S})) {
	$server = $options{S};
}
else {
	print "ERROR: madatory argument '-S' missing\n";
	usage();
	exit 1;
}

if (exists($options{s})) {
	$startmem = $options{s};
}

if (exists($options{n})) {
	$num_members = $options{n};
}

if (exists($options{m})) {
	$member_prefix = $options{m};
}
else {
	print "ERROR: mandatory argument '-m' missing\n";
	usage();
	exit 1;
}

if (exists($options{G})) {
	$startgroup = $options{G};
}

if (exists($options{N})) {
	$num_groups = $options{N};
}

if (exists($options{P})) {
	$path = $options{p};
	$net_path = "$path/$net_cmd";
}

if (@ARGV) {
	print "ERROR: junk on the command line ('" . join(" ", @ARGV) . "')...\n";
	usage();
	exit 1;
}

# utility functions:

sub do_add {
	my $member_name = shift;
	my $group_name = shift;
	print "adding member $member_name to group $group_name\n";
	system("$net_path rpc -I $server ".$creds." group addmem $group_name $member_name");
}

sub add_group_loop {
	my $member_name = shift;

	if ("x$startgroup" eq "x") {
		do_add($member_name, $group_prefix);
	}
	else {
		for (my $groupnum = 1; $groupnum <= $num_groups; ++$groupnum) {
			do_add($member_name, 
			       sprintf("%s%.05d", $group_prefix, $startgroup + $groupnum - 1));
		}
	}
}


# main:

if ("x$startmem" eq "x") {
	add_group_loop($member_prefix);
}
else {
	for (my $memnum = 1; $memnum <= $num_members; ++$memnum) {
		add_group_loop(sprintf("%s%.05d", $member_prefix, $startmem + $memnum - 1));
	}
}

