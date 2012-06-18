#!/usr/bin/perl

my %doc;

$invar = 0;

$topdir = (shift @ARGV) or $topdir = ".";

$progs = "";

open(IN, "$topdir/Makefile.in");
while(<IN>) {
	if($invar && /^([ \t]*)(.*?)([\\])$/) {
		$progs.=" " . $2;
		if($4) { $invar = 1; } else { $invar = 0; }
	} elsif(/^([^ ]*)_PROGS([0-9]*) = (.*?)([\\])$/) {
		$progs.=" " . $3;
		if($4) { $invar = 1; }
	} else { $invar = 0; }
}

foreach(split(/bin\//, $progs)) {
	next if($_ eq " ");
	s/\@EXEEXT\@//g;
	s/ //g;


	$f = $_;
	
	$found = 0;

	for($i = 0; $i < 9; $i++) {
		if(-e "manpages/$f.$i.xml") { $found = 1; }
	}

	if(!$found) {
		print "'$f' does not have a manpage\n";
	}
}
