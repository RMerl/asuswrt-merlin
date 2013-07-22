#!/usr/local/bin/perl
#
# count_pf.perl-- run lsof in repeat mode and count processes and
#		  files

sub interrupt { print "\n"; exit 0; }

$RPT = 15;				# lsof repeat time

# Set path to lsof.

if (($LSOF = &isexec("../lsof")) eq "") {	# Try .. first
    if (($LSOF = &isexec("lsof")) eq "") {	# Then try . and $PATH
	print "can't execute $LSOF\n"; exit 1
    }
}

# Read lsof -nPF output repeatedly from a pipe.

$| = 1;					# unbuffer output
$SIG{'INT'} = 'interrupt';		# catch interrupt
$proc = $files = $proto{'TCP'} = $proto{'UDP'} = 0;
$progress="/";				# used to show "progress"
open(P, "$LSOF -nPF -r $RPT|") || die "can't open pipe to $LSOF\n";

while (<P>) {
    chop;
    if (/^m/) {

    # A marker line signals the end of an lsof repetition.

	printf "%s  Processes: %5d,  Files: %6d,  TCP: %6d, UDP: %6d\r",
	    $progress, $proc, $files, $proto{'TCP'}, $proto{'UDP'};
	$proc = $files = $proto{'TCP'} = $proto{'UDP'} = 0;
	if ($progress eq "/") { $progress = "\\"; } else { $progress = "/"; }
	next;
    }
    if (/^p/) { $proc++; next; }		# Count processes.
    if (/^f/) { $files++; next; }		# Count files.
    if (/^P(.*)/) { $proto{$1}++; next; }	# Count protocols.
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
