#! /usr/bin/perl5
##
## This is a simple script written by Herb Lewis @ SGI <herb@samba.org>
## for reporting which parameters are supported by loadparm.c but 
## not by SWAT I just thought it looked fun and might be of interest to others
## --jerry@samba.org
##
## Here is a little info on the usage and output format so you don't have
## to dig through the code to understand what is printed.
##
## Useage: swat.pl [path_to_loadparm.c]
##
## The output consists of 4 columns of information
##     Option Name, Global Page, Share Page, Printer Page
## The section separaters will also be printed (preceded by 16 *) to show
## which options are grouped in the various sections.
##
## If the option name is preceded by an * it means this is a deprecated option.
## If the option name is preceded by 5 spaces it means this is an alias for the
## previous option.
##
## Under the Global Page, Share Page, and Printer Page columns there will be
## one of 3 entries, BASIC, ADVANCED, or no. "BASIC" indicates this option will
## show in the Basic View of that page in SWAT. "ADVANCED" indicates this
## option will show in the Advanced View of that page in SWAT. "No" indicates
## that this option is not available on that page in SWAT.
##
## Under the Global Page column, if an entry begins with an * it indicates that
## this is actually specified in Samba as a "service parameter" not a "global
## parameter" but you can set a default value for this on the Global Page in
## SWAT.
##
## --herb@samba.org

$lastone = "nothing";

if (@ARGV[0]) {
	$filename = @ARGV[0];
} else {
	$filename = "/usr3/samba20/samba/source/param/loadparm.c";
}

open (INFILE,$filename) || die "unable to open $filename\n";
while (not eof(INFILE))
{
	$_ = <INFILE>;
	last if ( /^static struct parm_struct parm_table/) ;
}
print "Option Name                     Global Page  Share Page  Printer Page\n";
print "---------------------------------------------------------------------";
while (not eof(INFILE))
{
	$_ = <INFILE>;
	last if (/};/);
	@fields = split(/,/,$_);
	next if not ($fields[0] =~ /^.*{"/);
	$fields[0] =~ s/.*{"//;
	$fields[0] =~ s/"//;
	if ($fields[3] eq $lastone) {
		print "     $fields[0]\n";
		next;
	}
	$lastone = $fields[3];
	$fields[2] =~ s/^\s+//;
	$fields[2] =~ s/\s+$//;
	$fields[2] =~ s/}.*$//;
	$fields[6] =~ s/^\s+//;
	$fields[6] =~ s/\s+$//;
	$fields[6] =~ s/}.*$//;
	if ($fields[2] =~ /P_SEPARATOR/) {
		print "\n****************$fields[0]\n";
		next;
	}
	else {
		if ($fields[6] =~ /FLAG_DEPRECATED/) {
			print "*$fields[0]".' 'x(31-length($fields[0]));
		}
		else {
			print "$fields[0]".' 'x(32-length($fields[0]));
		}
	}
	if (($fields[2] =~ /P_GLOBAL/) || ($fields[6] =~ /FLAG_GLOBAL/)) {
		if ($fields[6] =~ /FLAG_GLOBAL/) {
			print "*";
		}
		else {
			print " ";
		}
		if ($fields[6] =~ /FLAG_BASIC/) {
			print "BASIC       ";
		}
		else {
			print "ADVANCED    ";
		}
	}
	else {
		print " no          ";
	}
	if ($fields[6] =~ /FLAG_SHARE/) {
		if ($fields[6] =~ /FLAG_BASIC/) {
			print "BASIC       ";
		}
		else {
			print "ADVANCED    ";
		}
	}
	else {
		print "no          ";
	}
	if ($fields[6] =~ /FLAG_PRINT/) {
		if ($fields[6] =~ /FLAG_BASIC/) {
			print "BASIC";
		}
		else {
			print "ADVANCED";
		}
	}
	else {
		print "no";
	}
	print "\n";
}
