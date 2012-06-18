#!/usr/bin/perl
# Convenience functions for writing output expected by the buildfarm
# Copyright (C) 2009 Jelmer Vernooij <jelmer@samba.org>
# Published under the GNU GPL, v3 or later

package BuildFarm;

use Exporter;
@ISA = qw(Exporter);
@EXPORT_OK = qw(start_testsuite end_testsuite skip_testsuite summary);

use strict;

sub start_testsuite($$)
{
	my ($name, $duration) = @_;
	my $out = "";

	$out .= "--==--==--==--==--==--==--==--==--==--==--\n";
	$out .= "Running test $name (level 0 stdout)\n";
	$out .= "--==--==--==--==--==--==--==--==--==--==--\n";
	$out .= scalar(localtime())."\n";
	$out .= "SELFTEST RUNTIME: " . $duration . "s\n";
	$out .= "NAME: $name\n";

	print $out;
}

sub end_testsuite($$$$$)
{
	my ($name, $duration, $ok, $output, $reason) = @_;
	my $out = "";

	$out .= "TEST RUNTIME: " . $duration . "s\n";
	if ($ok) {
		$out .= "ALL OK\n";
	} else {
		$out .= "ERROR: $reason\n";
	}
	$out .= "==========================================\n";
	if ($ok) {
		$out .= "TEST PASSED: $name\n";
	} else {
		$out .= "TEST FAILED: $name (status $reason)\n";
	}
	$out .= "==========================================\n";

	print $out;
}

sub skip_testsuite($)
{
	my ($name) = @_;

	print "SKIPPED: $name\n";
}

sub summary($)
{
	my ($duration) = @_;

	print "DURATION: " . $duration . " seconds\n";
}

1;
