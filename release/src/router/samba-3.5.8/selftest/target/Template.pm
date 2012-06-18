#!/usr/bin/perl
# Copyright (C) 2009 Stefan Metzmacher <metze@samba.org>
# Published under the GNU GPL, v3 or later.

package Template;

use strict;

sub new($$) {
	my ($classname) = @_;
	my $self = {
		classname => $classname
	};
	bless $self;
	return $self;
}

sub teardown_env($$)
{
	my ($self, $envvars) = @_;

	return 0;
}

sub getlog_env($$)
{
	my ($self, $envvars) = @_;

	return "";
}

sub check_env($$)
{
	my ($self, $envvars) = @_;

	return 1;
}

sub setup_env($$$)
{
	my ($self, $envname, $path) = @_;
	my $envvars = undef;

	return $envvars;
}

sub stop($)
{
	my ($self) = @_;
}

1;
