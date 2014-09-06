# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use Test;
BEGIN { eval "use Cwd qw(abs_path)"; plan tests => 38 ; $ENV{'SNMPCONFPATH'} = 'nopath' ; $ENV{'MIBDIRS'} = '+' . abs_path("../../mibs"); };
use NetSNMP::OID;

ok(1); # If we made it this far, we're ok.

#########################

# Insert your test code below, the Test module is use()ed here so read
# its man page ( perldoc Test ) for help writing this test script.

my $oid = new NetSNMP::OID(".1.3.6.1");
ok(ref($oid) eq "NetSNMP::OID");
ok(ref($oid->{oidptr}) eq "netsnmp_oidPtr");
#print STDERR ref($oid),"\n";

my $tostring = "$oid";
#print STDERR "$tostring\n";
ok($tostring eq "internet");

my $oid2 = new NetSNMP::OID(".1.3.6.1.2");
$tostring = "$oid2";
#print STDERR "$tostring\n";
ok($tostring eq "mgmt");

my $oid3 = new NetSNMP::OID(".1.3.6.1");

my $val = NetSNMP::OID::snmp_oid_compare($oid, $oid2);
#print STDERR "compare result: $val\n";
ok($val == -1);

$val = $oid2->snmp_oid_compare($oid);
#print STDERR "compare result: $val\n";
ok($val == 1);

$val = NetSNMP::OID::compare($oid, $oid);
#print STDERR "compare result: $val\n";
ok($val == 0);

$val = $oid->compare($oid3);
#print STDERR "compare result: $val\n";
ok($val == 0);

ok(($oid <=> $oid2) == -1);
ok(($oid2 <=> $oid) == 1);
ok(($oid <=> $oid3) == 0);

ok($oid < $oid2);
ok($oid <= $oid2);
ok($oid2 > $oid);
ok($oid2 >= $oid);
ok($oid == $oid3);
ok($oid <= $oid3);
ok($oid >= $oid3);

ok(new NetSNMP::OID('system') < new NetSNMP::OID('interfaces'));
ok(new NetSNMP::OID('interfaces') > new NetSNMP::OID('system'));
ok(new NetSNMP::OID('sysORTable') > new NetSNMP::OID('system'));

my @a = $oid->to_array();
ok($a[0] == 1 && $a[1] == 3 && $a[2] == 6 && $a[3] == 1 && $#a == 3);

$oid->append(".1.2.3");
ok("$oid" eq "directory.2.3");

$oidmore = $oid + ".8.9.10";
ok($oidmore == new NetSNMP::OID("directory.2.3.8.9.10"));
ok("$oid" eq "directory.2.3");
ok(ref($oidmore) eq "NetSNMP::OID");

# += should work
$oidmore += ".11";
ok($oidmore == new NetSNMP::OID("directory.2.3.8.9.10.11"));

$oidstr = $oidmore + "\"wes\"";
ok($oidstr == new NetSNMP::OID("directory.2.3.8.9.10.11.3.119.101.115"));

$oidstr = $oidmore + "\'wes\'";
ok($oidstr == new NetSNMP::OID("directory.2.3.8.9.10.11.119.101.115"));

# just make sure you can do it twice (ie, not modify the original)
$oidstr = $oidmore + "\'wes\'";
ok($oidstr == new NetSNMP::OID("directory.2.3.8.9.10.11.119.101.115"));

$oidstr = $oidmore + "internet";
ok($oidstr == new NetSNMP::OID("directory.2.3.8.9.10.11.1.3.6.1"));

$oidstr = $oidmore + "999";
ok($oidstr == new NetSNMP::OID("directory.2.3.8.9.10.11.999"));

$oidstr = $oidmore + (new NetSNMP::OID(".1.3.6.1"));
ok($oidstr == new NetSNMP::OID("directory.2.3.8.9.10.11.1.3.6.1"));

$oid = new NetSNMP::OID("nosuchoidexists");
ok(ref($oid) ne "NetSNMP::OID");

ok($oidstr->length() == 15);

# multiple encoded values
my $newtest = new NetSNMP::OID ("nsModuleName.5.109.121.99.116.120.2.1.3.14");

if ($newtest) {
  my $arrayback = $newtest->get_indexes();
  
  ok($#$arrayback == 2 &&
    $arrayback->[0] eq 'myctx' &&
    $arrayback->[1] eq '.1.3' &&
    $arrayback->[2] eq '14'
  );
}
else {
  ok(0);
}
  
# implied string
$newtest = new NetSNMP::OID ("snmpNotifyRowStatus.105.110.116.101.114.110.97.108.48");

if ($newtest) {
  $arrayback = $newtest->get_indexes();
  
  ok($#$arrayback == 0 &&
    $arrayback->[0] eq 'internal0'
  );
}
else {
  ok(0);
}



