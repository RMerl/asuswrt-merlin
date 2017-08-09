# Perl module for parsing and generating the Subunit protocol
# Copyright (C) 2008-2009 Jelmer Vernooij <jelmer@samba.org>
#
#  Licensed under either the Apache License, Version 2.0 or the BSD 3-clause
#  license at the users choice. A copy of both licenses are available in the
#  project source as Apache-2.0 and BSD. You may not use this file except in
#  compliance with one of these two licences.
#  
#  Unless required by applicable law or agreed to in writing, software
#  distributed under these licenses is distributed on an "AS IS" BASIS, WITHOUT
#  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
#  license you chose for the specific language governing permissions and
#  limitations under that license.

package Subunit;
use POSIX;

require Exporter;
@ISA = qw(Exporter);
@EXPORT_OK = qw(parse_results $VERSION);

use vars qw ( $VERSION );

$VERSION = '0.0.2';

use strict;

sub parse_results($$$)
{
	my ($msg_ops, $statistics, $fh) = @_;
	my $expected_fail = 0;
	my $unexpected_fail = 0;
	my $unexpected_err = 0;
	my $open_tests = [];

	while(<$fh>) {
		if (/^test: (.+)\n/) {
			$msg_ops->control_msg($_);
			$msg_ops->start_test($1);
			push (@$open_tests, $1);
		} elsif (/^time: (\d+)-(\d+)-(\d+) (\d+):(\d+):(\d+)Z\n/) {
			$msg_ops->report_time(mktime($6, $5, $4, $3, $2, $1-1900));
		} elsif (/^(success|successful|failure|fail|skip|knownfail|error|xfail): (.*?)( \[)?([ \t]*)\n/) {
			$msg_ops->control_msg($_);
			my $result = $1;
			my $testname = $2;
			my $reason = undef;
			if ($3) {
				$reason = "";
				# reason may be specified in next lines
				my $terminated = 0;
				while(<$fh>) {
					$msg_ops->control_msg($_);
					if ($_ eq "]\n") { $terminated = 1; last; } else { $reason .= $_; }
				}
				
				unless ($terminated) {
					$statistics->{TESTS_ERROR}++;
					$msg_ops->end_test($testname, "error", 1, "reason ($result) interrupted");
					return 1;
				}
			}
			if ($result eq "success" or $result eq "successful") {
				pop(@$open_tests); #FIXME: Check that popped value == $testname 
				$statistics->{TESTS_EXPECTED_OK}++;
				$msg_ops->end_test($testname, $result, 0, $reason);
			} elsif ($result eq "xfail" or $result eq "knownfail") {
				pop(@$open_tests); #FIXME: Check that popped value == $testname
				$statistics->{TESTS_EXPECTED_FAIL}++;
				$msg_ops->end_test($testname, $result, 0, $reason);
				$expected_fail++;
			} elsif ($result eq "failure" or $result eq "fail") {
				pop(@$open_tests); #FIXME: Check that popped value == $testname
				$statistics->{TESTS_UNEXPECTED_FAIL}++;
				$msg_ops->end_test($testname, $result, 1, $reason);
				$unexpected_fail++;
			} elsif ($result eq "skip") {
				$statistics->{TESTS_SKIP}++;
				my $last = pop(@$open_tests);
				if (defined($last) and $last ne $testname) {
					push (@$open_tests, $testname);
				}
				$msg_ops->end_test($testname, $result, 0, $reason);
			} elsif ($result eq "error") {
				$statistics->{TESTS_ERROR}++;
				pop(@$open_tests); #FIXME: Check that popped value == $testname
				$msg_ops->end_test($testname, $result, 1, $reason);
				$unexpected_err++;
			} 
		} else {
			$msg_ops->output_msg($_);
		}
	}

	while ($#$open_tests+1 > 0) {
		$msg_ops->end_test(pop(@$open_tests), "error", 1,
				   "was started but never finished!");
		$statistics->{TESTS_ERROR}++;
		$unexpected_err++;
	}

	return 1 if $unexpected_err > 0;
	return 1 if $unexpected_fail > 0;
	return 0;
}

sub start_test($)
{
	my ($testname) = @_;
	print "test: $testname\n";
}

sub end_test($$;$)
{
	my $name = shift;
	my $result = shift;
	my $reason = shift;
	if ($reason) {
		print "$result: $name [\n";
		print "$reason";
		print "]\n";
	} else {
		print "$result: $name\n";
	}
}

sub skip_test($;$)
{
	my $name = shift;
	my $reason = shift;
	end_test($name, "skip", $reason);
}

sub fail_test($;$)
{
	my $name = shift;
	my $reason = shift;
	end_test($name, "fail", $reason);
}

sub success_test($;$)
{
	my $name = shift;
	my $reason = shift;
	end_test($name, "success", $reason);
}

sub xfail_test($;$)
{
	my $name = shift;
	my $reason = shift;
	end_test($name, "xfail", $reason);
}

sub report_time($)
{
	my ($time) = @_;
	my ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) = localtime($time);
	printf "time: %04d-%02d-%02d %02d:%02d:%02dZ\n", $year+1900, $mon, $mday, $hour, $min, $sec;
}

sub progress_pop()
{
	print "progress: pop\n";
}

sub progress_push()
{
	print "progress: push\n";
}

sub progress($;$)
{
	my ($count, $whence) = @_;

	unless(defined($whence)) {
		$whence = "";
	}

	print "progress: $whence$count\n";
}

1;
