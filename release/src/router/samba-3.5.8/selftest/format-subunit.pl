#!/usr/bin/perl
# Pretty-format subunit output
# Copyright (C) Jelmer Vernooij <jelmer@samba.org>
# Published under the GNU GPL, v3 or later

=pod

=head1 NAME

format-subunit [--format=<NAME>] [--immediate] < instream > outstream

=head1 SYNOPSIS

Format the output of a subunit stream.

=head1 OPTIONS

=over 4

=item I<--immediate>

Show errors as soon as they happen rather than at the end of the test run.

=item I<--format>=FORMAT

Choose the format to print. Currently supported are plain or html.

=head1 LICENSE

GNU General Public License, version 3 or later.

=head1 AUTHOR

Jelmer Vernooij <jelmer@samba.org>
		
=cut

use Getopt::Long;
use strict;
use FindBin qw($RealBin $Script);
use lib "$RealBin";
use Subunit qw(parse_results);

my $opt_format = "plain";
my $opt_help = undef;
my $opt_verbose = 0;
my $opt_immediate = 0;
my $opt_prefix = ".";

my $result = GetOptions (
		'help|h|?' => \$opt_help,
		'format=s' => \$opt_format,
		'verbose' => \$opt_verbose,
		'immediate' => \$opt_immediate,
		'prefix:s' => \$opt_prefix,
	    );

exit(1) if (not $result);

my $msg_ops;

my $statistics = {
	SUITES_FAIL => 0,

	TESTS_UNEXPECTED_OK => 0,
	TESTS_EXPECTED_OK => 0,
	TESTS_UNEXPECTED_FAIL => 0,
	TESTS_EXPECTED_FAIL => 0,
	TESTS_ERROR => 0,
	TESTS_SKIP => 0,
};

if ($opt_format eq "plain") {
	require output::plain;
	$msg_ops = new output::plain("$opt_prefix/summary", $opt_verbose, $opt_immediate, $statistics, undef);
} elsif ($opt_format eq "html") {
	require output::html;
	mkdir("test-results", 0777);
	$msg_ops = new output::html("test-results", $statistics);
} else {
	die("Invalid output format '$opt_format'");
}

my $expected_ret = parse_results($msg_ops, $statistics, *STDIN);

$msg_ops->summary();

exit($expected_ret);
