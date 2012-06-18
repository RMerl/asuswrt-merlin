#!/usr/bin/perl

use Test::More tests => 3;
use FindBin qw($RealBin);
use lib $RealBin;
use lib "$RealBin/target";
use Samba4;

my $s = new Samba4($RealBin."/../source4/bin", undef, $RealBin."/../setup");

ok($s);

is($RealBin."/../source4/bin", $s->{bindir});

ok($s->write_ldb_file("tmpldb", "
dn: a=b
a: b
c: d
"));

unlink("tmpldb");
