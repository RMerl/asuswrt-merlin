#!/usr/bin/perl
# Script that reads in Makefile.in and outputs the names of all 
# used but undefined vars and all defined but unused vars 
# Copyright Jelmer Vernooij <jelmer@samba.org>

# Arguments:
#  1: Makefile.in
#

my %references;
my %defines;

# First, make a list of defines in configure
$in = shift;

sub process_file($)
{
	my ($fn) = @_;
	open(IN, $fn);
	while(<IN>) {
		my $line = $_;
		while($line =~ /^\b([a-zA-Z0-9_][a-zA-Z0-9_]*)\b[ \t]*=.*/sgm) {
			$defines{$1} = 1;
		}
		while($line =~ /\$\(([a-zA-Z0-9_][a-zA-Z0-9_]*)\)/sgm) {
			$references{$1} = 1;
		}
		while ($line =~ /^include (.*)/sgm) {
			process_file($1);
		}
	}
	close IN;
}

process_file($in);

print "##### DEFINED BUT UNUSED: #####\n";
foreach(%defines) {
#    print $_." defined\n";

	if ($_ != 1) {
		if ($references{$_} != 1) {
			print $_."\n";
		}
	} 
}

print "##### USED BUT UNDEFINED: #####\n";
foreach(%references) {
	if ($_ != 1) {
		if ($defines{$_} != 1) {
			print $_."\n";
		}
	} 
}
