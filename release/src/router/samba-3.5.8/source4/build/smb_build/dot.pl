#!/usr/bin/perl
# Samba4 Dependency Graph Generator
# (C) 2004-2005 Jelmer Vernooij <jelmer@samba.org>
# Published under the GNU GPL

use strict;
use lib 'build';
use smb_build::config_mk;

my $subsys = shift @ARGV;

sub contains($$)
{
	my ($haystack,$needle) = @_;
	foreach (@$haystack) {
		return 1 if ($_ eq $needle);
	}
	return 0;
}

sub generate($$$)
{
	my ($depend,$only,$name) = @_;
	my $res = "digraph $name {\n";

	foreach my $part (values %{$depend}) {
		next if (defined($only) and not contains($only,$part->{NAME}));
		foreach my $elem (@{$part->{PUBLIC_DEPENDENCIES}}) {
			$res .= "\t\"$part->{NAME}\" -> \"$elem\" [style=filled]; /* public */\n";
		}
		foreach my $elem (@{$part->{PRIVATE_DEPENDENCIES}}) {
			$res .= "\t\"$part->{NAME}\" -> \"$elem\" [style=dotted]; /* private */\n";
		}
	}

	return $res . "}\n";
}

my $INPUT = {};
smb_build::config_mk::run_config_mk($INPUT, '.', '.', "main.mk");

my $name = "samba4";

my $only;
if (defined($subsys)) {
	my $DEPEND = smb_build::input::check($INPUT, \%config::enabled, 
		"MERGED_OBJ", "SHARED_LIBRARY", "SHARED_LIBRARY");

	die("No such subsystem $subsys") unless (defined($DEPEND->{$subsys}));

	$only = $DEPEND->{$subsys}->{UNIQUE_DEPENDENCIES_ALL};
	push (@$only, "$subsys");

	$name = $subsys;
}

my $fname = "$name-deps.dot";
print __FILE__.": creating $fname\n";
open DOTTY, ">$fname";
print DOTTY generate($INPUT, $only, $name);
close DOTTY;

1;
