#!/usr/local/bin/perl
#
# $Id: idrlogin.perl,v 1.5 2001/11/18 12:20:46 abe Exp $
#
# idrlogin.perl -- sample Perl script to identify the network source of a
#		   network (remote) login via rlogind, sshd, or telnetd 


# IMPORTANT DEFINITIONS
# =====================
#
# 1.  Set the interpreter line of this script to the local path of the
#     Perl executable.


#
# Copyright 1997 Purdue Research Foundation, West Lafayette, Indiana
# 47907.  All rights reserved.
#
# Written by Victor A. Abell
#
# This software is not subject to any license of the American Telephone
# and Telegraph Company or the Regents of the University of California.
#
# Permission is granted to anyone to use this software for any purpose on
# any computer system, and to alter it and redistribute it freely, subject
# to the following restrictions:
#
# 1. Neither the authors nor Purdue University are responsible for any
#    consequences of the use of this software.
#
# 2. The origin of this software must not be misrepresented, either by
#    explicit claim or by omission.  Credit to the authors and Purdue
#    University must appear in documentation and sources.
#
# 3. Altered versions must be plainly marked as such, and must not be
#    misrepresented as being the original software.
#
# 4. This notice may not be removed or altered.

# Initialize variables.

$dev = $name = $proto = "";					# fd variables
$fdst = 0;							# fd state
$pidst = 0;							# process state
$cmd = $login = $pid = $ppid = "";				# process var.

# Set path to lsof.

if (($LSOF = &isexec("../lsof")) eq "") {	# Try .. first
    if (($LSOF = &isexec("lsof")) eq "") {	# Then try . and $PATH
	print "can't execute $LSOF\n"; exit 1
    }
}

# Open a pipe from lsof.

open(P, "$LSOF -R -FcDfLpPRn|") || die "Can't pipe from $LSOF\n";

# Process the ``lsof -FcDfLpPRn'' output a line at a time

while (<P>) {
    chop;
    if (/^p(.*)/) {

# A process set begins with a PID field whose ID character is `p'.

	$tpid = $1;
	if ($pidst && $fdst) { &save_proc }
	$pidst = 1;
	$pid = $tpid;
	$cmd = $login = $ppid = "";
	$fdst = 0;
	$dev = $name = $proto = "";
	next;
    }

# Save process-related values.

    if (/^c(.*)/) { $cmd = $1; next; }
    if (/^L(.*)/) { $login = $1; next; }
    if (/^R(.*)/) { $ppid = $1; next; }

# A file set begins with a file descriptor field.

    if (/^f/) {
	if ($pidst && $fdst) { &save_proc }
	$fdst = 0;
	$dev = $name = $proto = "";
	next;
    }

# Accumulate file information.

    if (/^D(.*)/) { $dev = $1; next; }
    if (/^P(.*)/) { $proto = $1; next; }
    if (/^n(.*)/) { $name = $1; $fdst = 1; next; }
}

# Flush any stored file or process output.

if ($pidst && $fdst) { &save_proc }

# List the shell processes that have rlogind/sshd//telnetd parents.

$hdr = 0;
foreach $pid (sort keys(%shcmd)) {
    $p = $pid;
    if (!defined($raddr{$pid})) {
        for ($ff = 0; !$ff && defined($Ppid{$p}); ) {
	    $p = $Ppid{$p};
	    if ($p < 2 || defined($raddr{$p})) { $ff = 1; }
        }
    } else { $ff = 2; }
    if ($ff && defined($raddr{$p})) {
	if (!$hdr) {
	    printf "%-8.8s %-8.8s %6s %-10.10s %6s %-10.10s %s\n",
		"Login", "Shell", "PID", "Via", "PID", "TTY", "From";
	    $hdr = 1;
	}
	printf "%-8.8s %-8.8s %6d %-10.10s %6s %-10.10s %s\n",
	    $shlogin{$pid}, $shcmd{$pid}, $pid,
	    ($ff == 2) ? "(direct)" : $rcmd{$p},
	    ($ff == 2) ? "" : $p,
	    ($shtty{$pid} eq "") ? "(unknown)" : $shtty{$pid},
	    $raddr{$p};
    }
}
exit(0);


# save_proc -- save process information
#	       Values are stored inelegantly in global variables.

sub save_proc {
    if ($cmd eq ""
    ||  $login eq ""
    ||  $ppid eq ""
    ||  $pid eq ""
    ||  $name eq ""
    ) { return; }
    if (!defined($Ppid{$pid})) { $Ppid{$pid} = $ppid; }
    if ($proto eq "TCP"
    && (($cmd =~ /rlogind/) || ($cmd =~ /sshd/) || ($cmd =~ /telnetd/))) {
	if (defined($raddr{$pid})) { return; }
	if (($name =~ /[^:]*:[^-]*->([^:]*):.*/)) {
	    $raddr{$pid} = $1;
	    $rcmd{$pid} = $cmd;
	    return;
	}
    }
    if (($cmd =~ /.*sh$/)) {
	if (defined($shcmd{$pid})) { return; }
	if ($proto eq "TCP") {
	    if (defined($raddr{$pid})) { return; }
	    if (($name =~ /[^:]*:[^-]*->([^:]*):.*/)) {
		$raddr{$pid} = $1;
		$shcmd{$pid} = $cmd;
		$shlogin{$pid} = $login;
	    }
	}
	if (($name =~ m#/dev.*ty.*#)) {
	    ($tty) = ($name =~ m#/dev.*/(.*)#);
	} elsif (($name =~ m#/dev/(pts/\d+)#)) {
	    $tty = $1;
	} elsif (($name =~ m#/dev.*pts.*#)) {
	    $d = oct($dev);
	    $tty = sprintf("pts/%d", $d & 0xffff);
	} else { return; }
    } else { return; }
    $shcmd{$pid} = $cmd;
    $shtty{$pid} = $tty;
    $shlogin{$pid} = $login;
}


## isexec($path) -- is $path executable
#
# $path   = absolute or relative path to file to test for executabiity.
#	    Paths that begin with neither '/' nor '.' that arent't found as
#	    simple references are also tested with the path prefixes of the
#	    PATH environment variable.  

sub
isexec {
    my ($path) = @_;
    my ($i, @P, $PATH);

    $path =~ s/^\s+|\s+$//g;
    if ($path eq "") { return(""); }
    if (($path =~ m#^[\/\.]#)) {
	if (-x $path) { return($path); }
	return("");
    }
    $PATH = $ENV{PATH};
    @P = split(":", $PATH);
    for ($i = 0; $i <= $#P; $i++) {
	if (-x "$P[$i]/$path") { return("$P[$i]/$path"); }
    }
    return("");
}
