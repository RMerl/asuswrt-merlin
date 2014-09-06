#!/usr/bin/perl

# HEADER Perl DTLS/UDP Test

$agentaddress = "dtlsudp:localhost:9875";
$feature = "NETSNMP_TRANSPORT_TLSTCP_DOMAIN";

do "$ENV{'srcdir'}/testing/fulltests/tls/S300tlsperl.pl";
