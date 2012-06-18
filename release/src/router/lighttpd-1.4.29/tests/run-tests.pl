#! /usr/bin/env perl

use strict;

use Test::Harness qw(&runtests $verbose);
$verbose = (defined $ENV{'VERBOSE'} ? $ENV{'VERBOSE'} : 0);
my $tests = (defined $ENV{'RUNTESTS'} ? $ENV{'RUNTESTS'} : '');

my $srcdir = (defined $ENV{'srcdir'} ? $ENV{'srcdir'} : '.');

opendir DIR, $srcdir;
my (@fs, $f);
while ($f = readdir(DIR)) {
	if ($f =~ /^(.*)\.t$/) {
		next if ($tests ne '' and $tests !~ /(^|\s+)$1(\s+|$)/);
		push @fs, $srcdir.'/'.$f;
	}
}
closedir DIR;
runtests (sort @fs);
