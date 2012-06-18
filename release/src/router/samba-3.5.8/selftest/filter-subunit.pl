#!/usr/bin/perl
# Filter a subunit stream
# Copyright (C) Jelmer Vernooij <jelmer@samba.org>
# Published under the GNU GPL, v3 or later

=pod

=head1 NAME

filter-subunit - Filter a subunit stream

=head1 SYNOPSIS

filter-subunit --help

filter-subunit --prefix=PREFIX --known-failures=FILE < in-stream > out-stream

=head1 DESCRIPTION

Simple Subunit stream filter that will change failures to known failures 
based on a list of regular expressions.

=head1 OPTIONS

=over 4

=item I<--prefix>

Add the specified prefix to all test names.

=item I<--expected-failures>

Specify a file containing a list of tests that are expected to fail. Failures 
for these tests will be counted as successes, successes will be counted as 
failures.

The format for the file is, one entry per line:

TESTSUITE-NAME.TEST-NAME

The reason for a test can also be specified, by adding a hash sign (#) and the reason 
after the test name.

=head1 LICENSE

selftest is licensed under the GNU General Public License L<http://www.gnu.org/licenses/gpl.html>.


=head1 AUTHOR

Jelmer Vernooij

=cut

use Getopt::Long;
use strict;
use FindBin qw($RealBin $Script);
use lib "$RealBin";
use Subunit qw(parse_results);
use Subunit::Filter;

my $opt_expected_failures = undef;
my $opt_help = 0;
my $opt_prefix = undef;
my $opt_strip_ok_output = 0;
my @expected_failures = ();

my $result = GetOptions(
		'expected-failures=s' => \$opt_expected_failures,
		'strip-passed-output' => \$opt_strip_ok_output,
		'prefix=s' => \$opt_prefix,
		'help' => \$opt_help,
	);
exit(1) if (not $result);

if ($opt_help) {
	print "Usage: filter-subunit [--prefix=PREFIX] [--expected-failures=FILE]... < instream > outstream\n";
	exit(0);
}

if (defined($opt_expected_failures)) {
	@expected_failures = Subunit::Filter::read_test_regexes($opt_expected_failures);
}

my $statistics = {
	TESTS_UNEXPECTED_OK => 0,
	TESTS_EXPECTED_OK => 0,
	TESTS_UNEXPECTED_FAIL => 0,
	TESTS_EXPECTED_FAIL => 0,
	TESTS_ERROR => 0,
	TESTS_SKIP => 0,
};

my $msg_ops = new Subunit::Filter($opt_prefix, \@expected_failures, 
	                              $opt_strip_ok_output);

exit(parse_results($msg_ops, $statistics, *STDIN));
