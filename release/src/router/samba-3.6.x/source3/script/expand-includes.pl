#!/usr/bin/perl
# Expand the include lines in a Makefile
# Copyright (C) 2009 Jelmer Vernooij <jelmer@samba.org>
# Published under the GNU GPLv3 or later

my $depth = 0;

sub process($)
{
	my ($f) = @_;
	$depth++;
	die("Recursion in $f?") if ($depth > 100);
	open(IN, $f) or die("Unable to open $f: $!");
	foreach (<IN>) {
		my $l = $_;
		if ($l =~ /^include (.*)$/) {
			process($1);
		} else {
			print $l;
		}
	}
	$depth--;
}

my $path = shift;
unless ($path) {
	print STDERR "Usage: $0 Makefile.in > Makefile-noincludes.in\n";
	exit(1);
}
process($path);
