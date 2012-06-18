#!/usr/bin/perl
# Filter a subunit stream
# Copyright (C) Jelmer Vernooij <jelmer@samba.org>
# Published under the GNU GPL, v3 or later

package Subunit::Filter;

use strict;

sub read_test_regexes($)
{
	my ($name) = @_;
	my @ret = ();
	open(LF, "<$name") or die("unable to read $name: $!");
	while (<LF>) { 
		chomp; 
		next if (/^#/);
		if (/^(.*?)([ \t]+)\#([\t ]*)(.*?)$/) {
			push (@ret, [$1, $4]);
		} else {
			s/^(.*?)([ \t]+)\#([\t ]*)(.*?)$//;
			push (@ret, [$_, undef]); 
		}
	}
	close(LF);
	return @ret;
}

sub find_in_list($$)
{
	my ($list, $fullname) = @_;

	foreach (@$list) {
		if ($fullname =~ /$$_[0]/) {
			 return ($$_[1]) if ($$_[1]);
			 return "";
		}
	}

	return undef;
}

sub control_msg()
{
	# We regenerate control messages, so ignore this
}

sub report_time($$)
{
	my ($self, $time) = @_;
	Subunit::report_time($time);
}

sub output_msg($$)
{
	my ($self, $msg) = @_;
	unless(defined($self->{output})) {
		print $msg;
	} else {
		$self->{output}.=$msg;
	}
}

sub start_test($$)
{
	my ($self, $testname) = @_;

	if (defined($self->{prefix})) {
		$testname = $self->{prefix}.$testname;
	}

	if ($self->{strip_ok_output}) {
		$self->{output} = "";
	}

	Subunit::start_test($testname);
}

sub end_test($$$$$)
{
	my ($self, $testname, $result, $unexpected, $reason) = @_;

	if (defined($self->{prefix})) {
		$testname = $self->{prefix}.$testname;
	}

	if (($result eq "fail" or $result eq "failure") and not $unexpected) {
		$result = "xfail";
		$self->{xfail_added}++;
	}
	my $xfail_reason = find_in_list($self->{expected_failures}, $testname);
	if (defined($xfail_reason) and ($result eq "fail" or $result eq "failure")) {
		$result = "xfail";
		$self->{xfail_added}++;
		$reason .= $xfail_reason;
	}

	if ($self->{strip_ok_output}) {
		unless ($result eq "success" or $result eq "xfail" or $result eq "skip") {
			print $self->{output}
		}
	}
	$self->{output} = undef;

	Subunit::end_test($testname, $result, $reason);
}

sub skip_testsuite($;$)
{
	my ($self, $name, $reason) = @_;
	Subunit::skip_testsuite($name, $reason);
}

sub start_testsuite($;$)
{
	my ($self, $name) = @_;
	Subunit::start_testsuite($name);
	$self->{xfail_added} = 0;
}

sub end_testsuite($$;$)
{
	my ($self, $name, $result, $reason) = @_;
	if ($self->{xfail_added} and ($result eq "fail" or $result eq "failure")) {
		$result = "xfail";
	}
		
	Subunit::end_testsuite($name, $result, $reason);
}

sub testsuite_count($$)
{
	my ($self, $count) = @_;
	Subunit::testsuite_count($count);
}

sub new {
	my ($class, $prefix, $expected_failures, $strip_ok_output) = @_;

	my $self = { 
		prefix => $prefix,
		expected_failures => $expected_failures,
		strip_ok_output => $strip_ok_output,
		xfail_added => 0,
	};
	bless($self, $class);
}

1;
