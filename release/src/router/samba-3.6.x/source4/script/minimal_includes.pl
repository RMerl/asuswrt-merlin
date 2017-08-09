#!/usr/bin/perl -w
# find a list of #include lines in C code that might not be needed
# usually called with something like this:
#    minimal_includes.pl `find . -name "*.c"`
# Andrew Tridgell <tridge@samba.org>

use strict;
use Data::Dumper;
use Getopt::Long;

my $opt_help = 0;
my $opt_remove = 0;
my $opt_skip_system = 0;
my $opt_waf = 0;

#####################################################################
# write a string into a file
sub FileSave($$)
{
    my($filename) = shift;
    my($v) = shift;
    local(*FILE);
    open(FILE, ">$filename") || die "can't open $filename";    
    print FILE $v;
    close(FILE);
}

sub load_lines($)
{
	my $fname = shift;
	my @lines = split(/^/m, `cat $fname`);
	return @lines;
}

sub save_lines($$)
{
	my $fname = shift;
	my $lines = shift;
	my $data = join('', @{$lines});
	FileSave($fname, $data);
}

sub test_compile($)
{
	my $fname = shift;
	my $obj;
	if ($opt_waf) {
		my $ret = `../buildtools/bin/waf $fname 2>&1`;
		return $ret
	}
	if ($fname =~ s/(.*)\..*$/$1.o/) {
		$obj = "$1.o";
	} else {
		return "NOT A C FILE";
	}
	unlink($obj);
	my $ret = `make $obj 2>&1`;
	if (!unlink("$obj")) {
		return "COMPILE FAILED";
	}
	return $ret;
}

sub test_include($$$$)
{
	my $fname = shift;
	my $lines = shift;
	my $i = shift;
	my $original = shift;
	my $line = $lines->[$i];
	my $testfname;

	$lines->[$i] = "";

	my $mname = $fname . ".misaved";

	unlink($mname);
	rename($fname, $mname) || die "failed to rename $fname";
	save_lines($fname, $lines);
	
	my $out = test_compile($fname);

	if ($out eq $original) {
		if ($opt_remove) {
			if ($opt_skip_system && 
			    $line =~ /system\//) {
				print "$fname: not removing system include $line\n";
			} else {
				print "$fname: removing $line\n";
				unlink($mname);
				return;
			}
		} else {
			print "$fname: might be able to remove $line\n";
		}
	}

	$lines->[$i] = $line;
	rename($mname, $fname) || die "failed to restore $fname";
}

sub process_file($)
{
	my $fname = shift;
	my @lines = load_lines($fname);
	my $num_lines = $#lines;

	my $original = test_compile($fname);

	if ($original eq "COMPILE FAILED") {
		print "Failed to compile $fname\n";
		return;
	}

	print "Processing $fname (with $num_lines lines)\n";
	
	my $if_level = 0;

	for (my $i=0;$i<=$num_lines;$i++) {
		my $line = $lines[$i];
		if ($line =~ /^\#\s*if/) {
			$if_level++;
		}
		if ($line =~ /^\#\s*endif/) {
			$if_level--;
		}
		if ($if_level == 0 &&
		    $line =~ /^\#\s*include/ && 
		    !($line =~ /needed/)) {
			test_include($fname, \@lines, $i, $original);
		}
	}
}


#########################################
# display help text
sub ShowHelp()
{
    print "
           minimise includes
           Copyright (C) tridge\@samba.org

	   Usage: minimal_includes.pl [options] <C files....>
	   
	   Options:
                 --help         show help
                 --remove       remove includes, don't just list them
                 --skip-system  don't remove system/ includes
                 --waf          use waf target conventions
";
}


# main program
GetOptions (
	    'h|help|?' => \$opt_help,
	    'remove' => \$opt_remove,
	    'skip-system' => \$opt_skip_system,
	    'waf' => \$opt_waf,
	    );

if ($opt_help) {
	ShowHelp();
	exit(0);
}

for (my $i=0;$i<=$#ARGV;$i++) {
	my $fname = $ARGV[$i];
	process_file($fname);
}
