#!/usr/bin/perl
# HTML output for selftest
# Copyright (C) 2008 Jelmer Vernooij <jelmer@samba.org>
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
package output::html;
use Exporter;
@ISA = qw(Exporter);

use strict;
use warnings;

use FindBin qw($RealBin);
use lib "$RealBin/..";

sub new($$$) {
	my ($class, $dirname, $statistics) = @_;
	my $self = { 
		dirname => $dirname,
		active_test => undef,
		local_statistics => {},
		statistics => $statistics,
		msg => "",
		error_summary => { 
			skip => [],
			expected_success => [],
			unexpected_success => [],
			expected_failure => [],
			unexpected_failure => [],
			skip_testsuites => [],
			error => []
		}
	};

	link("$RealBin/output/testresults.css", "$dirname/testresults.css");

	open(INDEX, ">$dirname/index.html");

	bless($self, $class);

	$self->print_html_header("Samba Testsuite Run", *INDEX);

	print INDEX "  <center>";
	print INDEX "  <table>\n";
	print INDEX "  <tr>\n";
	print INDEX "    <td class=\"tableHead\">Test</td>\n";
	print INDEX "    <td class=\"tableHead\">Result</td>\n";
	print INDEX "  </tr>\n";

	return $self;
}

sub testsuite_count($$)
{
}

sub print_html_header($$$)
{
	my ($self, $title, $fh) = @_;

	print $fh "<html lang=\"en\">\n";
	print $fh "<head>\n";
	print $fh "  <title>$title</title>\n";
	print $fh "  <link rel=\"stylesheet\" type=\"text/css\" href=\"testresults.css\"/>\n";
	print $fh "</head>\n";
	print $fh "<body>\n";
	print $fh "<table width=\"100%\" border=\"0\" cellspacing=\"0\">\n";
	print $fh "  <tr><td class=\"title\">$title</td></tr>\n";
	print $fh "  <tr><td>\n";
}

sub print_html_footer($$)
{
	my ($self, $fh) = @_;

	print $fh "</td></tr>\n";
	print $fh "</table>\n";
	print $fh "</body>\n";
	print $fh "</html>\n";
}

sub output_msg($$);

sub start_testsuite($$)
{
	my ($self, $name) = @_;

	$self->{START_TIME} = $self->{last_time};

	$self->{local_statistics} = {
		success => 0,
		skip => 0,
		error => 0,
		failure => 0
	};

	$self->{NAME} = $name;
	$self->{HTMLFILE} = "$name.html";
	$self->{HTMLFILE} =~ s/[:\t\n \/]/_/g;

	open(TEST, ">$self->{dirname}/$self->{HTMLFILE}") or die("Unable to open $self->{HTMLFILE} for writing");

	$self->print_html_header("Test Results for $name", *TEST);

	print TEST "<h2>Tests</h2>\n";

	print TEST "  <table>\n";
}

sub control_msg($$)
{
	my ($self, $output) = @_;

	# Perhaps the CSS should hide this by default?
	$self->{msg} .=  "<span class=\"control\">$output<br/></span>\n";
}

sub output_msg($$)
{
	my ($self, $output) = @_;

	unless (defined($self->{active_test})) {
		if (defined($self->{NAME})) {
			print TEST "$output<br/>";
		}
	} else {
		$self->{msg} .= "$output<br/>";
	}
}

sub end_testsuite($$$)
{
	my ($self, $name, $result, $reason) = @_;

	print TEST "</table>\n";

	print TEST "<div class=\"duration\">Duration: " . ($self->{last_time} - $self->{START_TIME}) . "s</div>\n";

	$self->print_html_footer(*TEST);

	close(TEST);

	print INDEX "<tr>\n";
	print INDEX "  <td class=\"testSuite\"><a href=\"$self->{HTMLFILE}\">$name</a></td>\n";
	my $st = $self->{local_statistics};

	if ($result eq "xfail") {
		print INDEX "  <td class=\"resultExpectedFailure\">";
	} elsif ($result eq "success") {
		print INDEX "  <td class=\"resultOk\">";
	} else {
		print INDEX "  <td class=\"resultFailure\">";
	}

	my $l = 0;
	if ($st->{success} > 0) {
		print INDEX "$st->{success} ok";
		$l++;
	}
	if ($st->{skip} > 0) {
		print INDEX ", " if ($l);
		print INDEX "$st->{skip} skipped";
		$l++;
	}
	if ($st->{failure} > 0) {
		print INDEX ", " if ($l);
		print INDEX "$st->{failure} failures";
		$l++;
	}
	if ($st->{error} > 0) {
		print INDEX ", " if ($l);
		print INDEX "$st->{error} errors";
		$l++;
	}

	if ($l == 0) {
		print INDEX uc($result);
	}

	print INDEX "</td>";
		
	print INDEX "</tr>\n";

	$self->{NAME} = undef;
}

sub report_time($$)
{
	my ($self, $time) = @_;
	$self->{last_time} = $time;
}

sub start_test($$)
{
	my ($self, $testname) = @_;

	$self->{active_test} = $testname;
	$self->{msg} = "";
}

sub end_test($$$$)
{
	my ($self, $testname, $result, $unexpected, $reason) = @_;

	print TEST "<tr>";

	$self->{local_statistics}->{$result}++;

	my $track_class;

	if ($result eq "skip") {
		print TEST "<td class=\"outputSkipped\">\n";
		$track_class = "skip";
	} elsif ($unexpected) {
		print TEST "<td class=\"outputFailure\">\n";
		if ($result eq "error") {
			$track_class = "error";
		} else {
			$track_class = "unexpected_$result";
		}
	} else {
		if ($result eq "failure") {
			print TEST "<td class=\"outputExpectedFailure\">\n";
		} else {
			print TEST "<td class=\"outputOk\">\n";
		}
		$track_class = "expected_$result";
	}

	push(@{$self->{error_summary}->{$track_class}}, ,
		 [$self->{HTMLFILE}, $testname, $self->{NAME}, 
		  $reason]);

	print TEST "<a name=\"$testname\"><h3>$testname</h3></a>\n";

	print TEST $self->{msg};

	if (defined($reason)) {
		print TEST "<div class=\"reason\">$reason</div>\n";
	}

	print TEST "</td></tr>\n";

	$self->{active_test} = undef;
}

sub summary($)
{
	my ($self) = @_;

	my $st = $self->{statistics};
	print INDEX "<tr>\n";
	print INDEX "  <td class=\"testSuiteTotal\">Total</td>\n";

	if ($st->{TESTS_UNEXPECTED_OK} == 0 and 
	    $st->{TESTS_UNEXPECTED_FAIL} == 0 and
		$st->{TESTS_ERROR} == 0) {
		print INDEX "  <td class=\"resultOk\">";
	} else {
		print INDEX "  <td class=\"resultFailure\">";
	}
	print INDEX ($st->{TESTS_EXPECTED_OK} + $st->{TESTS_UNEXPECTED_OK}) . " ok";
	if ($st->{TESTS_UNEXPECTED_OK} > 0) {
		print INDEX " ($st->{TESTS_UNEXPECTED_OK} unexpected)";
	}
	if ($st->{TESTS_SKIP} > 0) {
		print INDEX ", $st->{TESTS_SKIP} skipped";
	}
	if (($st->{TESTS_UNEXPECTED_FAIL} + $st->{TESTS_EXPECTED_FAIL}) > 0) {
		print INDEX ", " . ($st->{TESTS_UNEXPECTED_FAIL} + $st->{TESTS_EXPECTED_FAIL}) . " failures";
		if ($st->{TESTS_UNEXPECTED_FAIL} > 0) {
			print INDEX " ($st->{TESTS_EXPECTED_FAIL} expected)";
		}
	}
	if ($st->{TESTS_ERROR} > 0) {
		print INDEX ", $st->{TESTS_ERROR} errors";
	}

	print INDEX "</td>";

	print INDEX "</tr>\n";

	print INDEX "</table>\n";
	print INDEX "<a href=\"summary.html\">Summary</a>\n";
	print INDEX "</center>\n";
	$self->print_html_footer(*INDEX);
	close(INDEX);

	my $summ = $self->{error_summary};
	open(SUMMARY, ">$self->{dirname}/summary.html");
	$self->print_html_header("Summary", *SUMMARY);
	sub print_table($$) {
		my ($title, $list) = @_;
		return if ($#$list == -1);
		print SUMMARY "<h3>$title</h3>\n";
		print SUMMARY "<table>\n";
		print SUMMARY "<tr>\n";
		print SUMMARY "  <td class=\"tableHead\">Testsuite</td>\n";
		print SUMMARY "  <td class=\"tableHead\">Test</td>\n";
		print SUMMARY "  <td class=\"tableHead\">Reason</td>\n";
		print SUMMARY "</tr>\n";

		foreach (@$list) {
			print SUMMARY "<tr>\n";
			print SUMMARY "  <td><a href=\"" . $$_[0] . "\">$$_[2]</a></td>\n";
			print SUMMARY "  <td><a href=\"" . $$_[0] . "#$$_[1]\">$$_[1]</a></td>\n";
			if (defined($$_[3])) {
				print SUMMARY "  <td>$$_[3]</td>\n";
			} else {
				print SUMMARY "  <td></td>\n";
			}
			print SUMMARY "</tr>\n";
		}

		print SUMMARY "</table>";
	}
	print_table("Errors", $summ->{error});
	print_table("Unexpected successes", $summ->{unexpected_success});
	print_table("Unexpected failures", $summ->{unexpected_failure});
	print_table("Skipped tests", $summ->{skip});
	print_table("Expected failures", $summ->{expected_failure});

	print SUMMARY "<h3>Skipped testsuites</h3>\n";
	print SUMMARY "<table>\n";
	print SUMMARY "<tr>\n";
	print SUMMARY "  <td class=\"tableHead\">Testsuite</td>\n";
	print SUMMARY "  <td class=\"tableHead\">Reason</td>\n";
	print SUMMARY "</tr>\n";

	foreach (@{$summ->{skip_testsuites}}) {
		print SUMMARY "<tr>\n";
		print SUMMARY "  <td>$$_[0]</td>\n";
		if (defined($$_[1])) {
			print SUMMARY "  <td>$$_[1]</td>\n";
		} else {
			print SUMMARY "  <td></td>\n";
		}
		print SUMMARY "</tr>\n";
	}

	print SUMMARY "</table>";

	$self->print_html_footer(*SUMMARY);
	close(SUMMARY);
}

sub skip_testsuite($$$$)
{
	my ($self, $name, $reason) = @_;

	push (@{$self->{error_summary}->{skip_testsuites}}, 
		  [$name, $reason]);
}

1;
