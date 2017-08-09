#!/usr/bin/perl
# Script that reads in C files and prints defines that are used nowhere in the 
# code

# Arguments: C and H files
# Copyright Jelmer Vernooij <jelmer@samba.org>, GPL

use strict;

my %defined;
my %used;
my %files;

my $tmp;
while($tmp = shift) { 
	$files{$tmp} = $tmp;
	open(FI, $tmp);
	my $ln = 0;
	while(<FI>) { 
		$ln++;
		my $line = $_;
		my $cur = "";
		if(/^#define ([A-Za-z0-9_]+)/) {
			$defined{$1} = "$tmp:$ln";
			$cur = $1;
		}

		$_ = $line;
		while(/([A-Za-z0-9_]+)/sgm) { 
			if($cur ne $1) { $used{$1} = "$tmp:$ln"; }
		}
	}
	close FI;
}

foreach(keys %defined) {
	if(!$used{$_}) { print "$defined{$_}: Macro `$_' is unused\n"; }
}
