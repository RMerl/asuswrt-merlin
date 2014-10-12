#!/usr/bin/perl
# Bootstrap Samba and run a number of tests against it.
# Copyright (C) 2005-2007 Jelmer Vernooij <jelmer@samba.org>
# Published under the GNU GPL, v3 or later.

package Windows;

use strict;
use FindBin qw($RealBin);
use POSIX;

sub new($)
{
	my ($classname) = @_;
	my $self = { };
	bless $self;
	return $self;
}

sub provision($$$)
{
	my ($self, $environment, $prefix) = @_;

	die ("Windows tests will not run without root privileges.") 
		if (`whoami` ne "root");

	die("Environment variable WINTESTCONF has not been defined.\n".
		"Windows tests will not run unconfigured.") if (not defined($ENV{WINTESTCONF}));

	die ("$ENV{WINTESTCONF} could not be read.") if (! -r $ENV{WINTESTCONF});

	$ENV{WINTEST_DIR}="$ENV{SRCDIR}/selftest/win";
}

sub setup_env($$)
{
	my ($self, $name) = @_;
}

1;
