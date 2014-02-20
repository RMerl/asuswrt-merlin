#!/usr/local/bin/perl
#
# watch_a_file.perl -- use lsof -F output to watch a specific file
#		       (or file system)
#
# usage:	watch_a_file.perl file_name

## Interrupt handler

sub interrupt { wait; print "\n"; exit 0; }


## Start main program

$Pn = "watch_a_file";
# Check file argument.

if ($#ARGV != 0) { print "$#ARGV\n"; die "$Pn usage: file_name\n"; }
$fnm = $ARGV[0];
if (! -r $fnm) { die "$Pn: can't read $fnm\n"; }

# Do setup.

$RPT = 15;				# lsof repeat time
$| = 1;					# unbuffer output
$SIG{'INT'} = 'interrupt';		# catch interrupt

# Set path to lsof.

if (($LSOF = &isexec("../lsof")) eq "") {	# Try .. first
    if (($LSOF = &isexec("lsof")) eq "") {	# Then try . and $PATH
	print "can't execute $LSOF\n"; exit 1
    }
}

# Read lsof -nPF output from a pipe and gather the PIDs of the processes
# and file descriptors to watch.

open(P, "$LSOF -nPFpf $fnm|") || die "$Pn: can't pipe to $LSOF\n";

$curpid = -1;
$pids = "";
while (<P>) {
    chop;
    if (/^p(.*)/) { $curpid = $1; next; }	# Identify process.
    if (/^f/) {
	if ($curpid > 0) {
	    if ($pids eq "") { $pids = $curpid; }
	    else { $pids = $pids . "," . $curpid; }
	    $curpid = -1;
	}
    }
}
close(P);
wait;
if ($pids eq "") { die "$Pn: no processes using $fnm located.\n"; }
print "watch_file: $fnm being used by processes:\n\t$pids\n\n";

# Read repeated lsof output from a pipe and display.

$pipe = "$LSOF -ap $pids -r $RPT $fnm";
open(P, "$pipe|") || die "$Pn: can't pipe: $pipe\n";

while (<P>) { print $_; }
close(P);
print "$Pn: unexpected EOF from \"$pipe\"\n";
exit 1;


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
