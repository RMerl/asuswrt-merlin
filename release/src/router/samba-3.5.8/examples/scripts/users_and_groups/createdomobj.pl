#!/usr/bin/perl

#
# createdomobj.pl
#
#    create single or continuously numbered domain 
#    users/groups/aliases via rpc
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


my $target_type	= "group";		# what type of object to create
my $rpc_cmd	= "createdom".$target_type;
my $rpccli_cmd	= "rpcclient";

# defaults:

my $server;
my $num_targets	= 1;
my $startnum;				# if empty, don't add numbers to prefix
my $prefix	= $target_type;		# name-prefix
my $path;				# path to rpcclient command
my $rpccli_path	= $rpccli_cmd;
my $creds;

sub usage {
	print "USAGE: $0 [-h] -S server -U user\%pass [-p prefix] \\\n"
		. "\t[-t {alias|group|user}] [-s startnum] [-n numobjs] [-P path] \n";
}

# parse commandline:

my %options = ();
getopts("U:t:S:s:n:p:P:h", \%options);

if (exists($options{h})) {
	usage();
	exit 0;
}

if (exists($options{t})) {
	$target_type = $options{t};
	if ($target_type !~ /^(alias|user|group)$/) {
		print "ERROR: invalid target type given\n";
		usage();
		exit 1;
	}
	$rpc_cmd = "createdom".$target_type;
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
	$startnum = $options{s};
}

if (exists($options{n})) {
	$num_targets = $options{n};
}

if (exists($options{p})) {
	$prefix = $options{p};
}

if (exists($options{P})) {
	$path = $options{p};
	$rpccli_path = "$path/$rpccli_cmd";
}

if (@ARGV) {
	print "ERROR: junk on the command line ('" . join(" ", @ARGV) . "')...\n";
	usage();
	exit 1;
}

# utility functions:

sub open_rpc_pipe {
	print "opening rpc pipe\n";
	open(IPC, "| $rpccli_cmd $server $creds -d0") or
		die "error opening rpc pipe.";
}

sub close_rpc_pipe {
	print "closing rpc pipe\n";
	close(IPC);
}

sub do_create {
	my $target_name = shift;
	print "creating $target_type $target_name\n";
	print IPC "$rpc_cmd $target_name\n";
}

# main:

open_rpc_pipe();

if ("x$startnum" eq "x") {
	do_create($prefix);
}
else {
	for (my $num = 1; $num <= $num_targets; ++$num) {
		do_create(sprintf "%s%.05d", $prefix, $startnum + $num - 1);
		if (($num) % 500 == 0) {
			printf("500 ".$target_type."s created\n");
			close_rpc_pipe();
			sleep 2;
			open_rpc_pipe();
		}
	}
}

close_rpc_pipe();

