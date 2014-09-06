#!/usr/bin/perl

# HEADER Perl Unix Domain Socket Test

BEGIN {
    if (exists($ENV{'srcdir'})) {
	push @INC, "$ENV{'srcdir'}/testing/fulltests/perl";
    } elsif (-d "fulltests/perl") {
	push @INC, "fulltests/perl";
    } elsif (-d "../perl") {
	push @INC, "../perl";
    }
}
use NetSNMPTestTransport;

my $test = new NetSNMPTestTransport(agentaddress => "bogus");
$test->require_feature("NETSNMP_TRANSPORT_UNIX_DOMAIN");
$test->{'agentaddress'} = "unix:" . $test->{'dir'} . "/unixtestsocket";
$test->run_tests();
