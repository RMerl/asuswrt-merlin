#!/usr/bin/perl
# Diff two subunit streams
# Copyright (C) Jelmer Vernooij <jelmer@samba.org>
# Published under the GNU GPL, v3 or later

use Getopt::Long;
use strict;
use FindBin qw($RealBin $Script);
use lib "$RealBin";
use Subunit::Diff;

my $old = Subunit::Diff::from_file($ARGV[0]);
my $new = Subunit::Diff::from_file($ARGV[1]);

my $ret = Subunit::Diff::diff($old, $new);

foreach my $e (sort(keys %$ret)) {
	printf "%s: %s -> %s\n", $e, $ret->{$e}[0], $ret->{$e}[1];
}

0;
