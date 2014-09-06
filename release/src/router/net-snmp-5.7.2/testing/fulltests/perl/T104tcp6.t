#!/usr/bin/perl

# HEADER Perl TCP IPv6 Test

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

my $test = new NetSNMPTestTransport(agentaddress => "tcp6:[::1]:9875");
$test->require_feature("NETSNMP_TRANSPORT_TCPIPV6_DOMAIN");
$test->run_tests();
