#!/usr/bin/perl

# HEADER Perl TLS/TCP Test

$agentaddress = "tlstcp:localhost:9875";
$feature = "NETSNMP_TRANSPORT_TLSTCP_DOMAIN";

do "$ENV{'srcdir'}/testing/fulltests/tls/S300tlsperl.pl";
