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

use strict;

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
		print $reason;
		if (substr($reason, -1, 1) ne "\n") { print "\n"; }
		print "]\n";
	} else {
		print "$result: $name\n";
	}
}

sub report_time($)
{
	my ($time) = @_;
	my ($sec, $min, $hour, $mday, $mon, $year, $wday, $yday, $isdst) = localtime($time);
	$sec = ($time - int($time) + $sec);
	my $msg = sprintf("%f", $sec);
	if (substr($msg, 1, 1) eq ".") {
		$msg = "0" . $msg;
	}
	printf "time: %04d-%02d-%02d %02d:%02d:%s\n", $year+1900, $mon+1, $mday, $hour, $min, $msg;
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

1;
