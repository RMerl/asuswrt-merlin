# Perl module for parsing and generating the Subunit protocol
# Copyright (C) 2008-2009 Jelmer Vernooij <jelmer@samba.org>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

package Subunit;
use POSIX;

require Exporter;
@ISA = qw(Exporter);
@EXPORT_OK = qw(parse_results);

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
		} elsif (/^time: (\d+)-(\d+)-(\d+) (\d+):(\d+):(\d+)\n/) {
			$msg_ops->report_time(mktime($6, $5, $4, $3, $2-1, $1-1900));
		} elsif (/^(success|successful|failure|fail|skip|knownfail|error|xfail|skip-testsuite|testsuite-failure|testsuite-xfail|testsuite-success|testsuite-error): (.*?)( \[)?([ \t]*)\n/) {
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
					$msg_ops->end_test($testname, "error", 1, 
						               "reason ($result) interrupted");
					return 1;
				}
			}
			if ($result eq "success" or $result eq "successful") {
				pop(@$open_tests); #FIXME: Check that popped value == $testname 
				$statistics->{TESTS_EXPECTED_OK}++;
				$msg_ops->end_test($testname, "success", 0, $reason);
			} elsif ($result eq "xfail" or $result eq "knownfail") {
				pop(@$open_tests); #FIXME: Check that popped value == $testname
				$statistics->{TESTS_EXPECTED_FAIL}++;
				$msg_ops->end_test($testname, "xfail", 0, $reason);
				$expected_fail++;
			} elsif ($result eq "failure" or $result eq "fail") {
				pop(@$open_tests); #FIXME: Check that popped value == $testname
				$statistics->{TESTS_UNEXPECTED_FAIL}++;
				$msg_ops->end_test($testname, "failure", 1, $reason);
				$unexpected_fail++;
			} elsif ($result eq "skip") {
				$statistics->{TESTS_SKIP}++;
				# Allow tests to be skipped without prior announcement of test
				my $last = pop(@$open_tests);
				if (defined($last) and $last ne $testname) {
					push (@$open_tests, $testname);
				}
				$msg_ops->end_test($testname, "skip", 0, $reason);
			} elsif ($result eq "error") {
				$statistics->{TESTS_ERROR}++;
				pop(@$open_tests); #FIXME: Check that popped value == $testname
				$msg_ops->end_test($testname, "error", 1, $reason);
				$unexpected_err++;
			} elsif ($result eq "skip-testsuite") {
				$msg_ops->skip_testsuite($testname);
			} elsif ($result eq "testsuite-success") {
				$msg_ops->end_testsuite($testname, "success", $reason);
			} elsif ($result eq "testsuite-failure") {
				$msg_ops->end_testsuite($testname, "failure", $reason);
			} elsif ($result eq "testsuite-xfail") {
				$msg_ops->end_testsuite($testname, "xfail", $reason);
			} elsif ($result eq "testsuite-error") {
				$msg_ops->end_testsuite($testname, "error", $reason);
			} 
		} elsif (/^testsuite: (.*)\n/) {
			$msg_ops->start_testsuite($1);
		} elsif (/^testsuite-count: (\d+)\n/) {
			$msg_ops->testsuite_count($1);
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
	printf "time: %04d-%02d-%02d %02d:%02d:%02d\n", $year+1900, $mon+1, $mday, $hour, $min, $sec;
}

# The following are Samba extensions:

sub start_testsuite($)
{
	my ($name) = @_;
	print "testsuite: $name\n";
}

sub skip_testsuite($;$)
{
	my ($name, $reason) = @_;
	if ($reason) {
		print "skip-testsuite: $name [\n$reason\n]\n";
	} else {
		print "skip-testsuite: $name\n";
	}
}

sub end_testsuite($$;$)
{
	my $name = shift;
	my $result = shift;
	my $reason = shift;
	if ($reason) {
		print "testsuite-$result: $name [\n";
		print "$reason\n";
		print "]\n";
	} else {
		print "testsuite-$result: $name\n";
	}
}

sub testsuite_count($)
{
	my ($count) = @_;
	print "testsuite-count: $count\n";
}

1;
