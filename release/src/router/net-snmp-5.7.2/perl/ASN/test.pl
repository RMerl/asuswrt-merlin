# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; print "1..9\n"; }
END {print "not ok 1\n" unless $loaded;}
use NetSNMP::ASN (':all');
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

# Insert your test code below (better if it prints "ok 13"
# (correspondingly "not ok 13") depending on the success of chunk 13
# of the test code):

print ((ASN_INTEGER == 2) ? "ok 2\n" : "not ok 2\n");
print ((ASN_OCTET_STR == 4) ? "ok 3\n" : "not ok 3\bn");
print ((ASN_COUNTER == 0x41) ? "ok 4\n" : "not ok 4\n");
print ((ASN_UNSIGNED == 0x42) ? "ok 5\n" : "not ok 5\n");
print ((ASN_COUNTER64 == 0x46) ? "ok 6\n" : "not ok 6\n");
print ((ASN_IPADDRESS == 0x40) ? "ok 7\n" : "not ok 7\n");
print ((ASN_NULL == 5) ? "ok 8\n" : "not ok 8\n");
print ((ASN_TIMETICKS == 0x43) ? "ok 9\n" : "not ok 9\n");
