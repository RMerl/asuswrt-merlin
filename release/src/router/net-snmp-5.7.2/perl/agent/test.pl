# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

######################### We start with some black magic to print on failure.

# Change 1..1 below to 1..last_test_to_print .
# (It may become useful if the test is moved to ./t subdirectory.)

BEGIN { $| = 1; 
        $ENV{'SNMPCONFPATH'} = 'nopath';
        $ENV{'MIBS'} = '';
        print "1..6\n";
      }
END {print "not ok 1\n" unless $loaded;}
use NetSNMP::agent (':all');
use NetSNMP::default_store (':all');
use NetSNMP::agent::default_store (':all');
use NetSNMP::ASN (':all');
use NetSNMP::OID;
#use NetSNMP::agent (':all');
use SNMP;
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

sub it {
    if ($_[0]) {
	return "ok " . $_[1] . "\n";
    } else {
	return "not ok ". $_[1] ."\n";
    }
}

# Insert your test code below (better if it prints "ok 13"
# (correspondingly "not ok 13") depending on the success of chunk 13
# of the test code):

print it((MODE_GET == 0xa0 &&
	  MODE_GETNEXT == 0xa1 &&
	  MODE_GETBULK == 0xa5 &&
	  MODE_SET_BEGIN == -1 &&
	  MODE_SET_RESERVE1 == 0 &&
	  MODE_SET_RESERVE2 == 1 &&
	  MODE_SET_ACTION == 2 &&
	  MODE_SET_COMMIT == 3 &&
	  MODE_SET_FREE == 4 &&
	  MODE_SET_UNDO == 5 &&
	  SNMP_ERR_NOERROR == 0 &&
	  SNMP_ERR_TOOBIG == 1 &&
	  SNMP_ERR_NOSUCHNAME == 2 &&
	  SNMP_ERR_BADVALUE == 3 &&
	  SNMP_ERR_READONLY == 4 &&
	  SNMP_ERR_GENERR == 5 &&
	  SNMP_ERR_NOACCESS == 6 &&
	  SNMP_ERR_WRONGTYPE == 7 &&
	  SNMP_ERR_WRONGLENGTH == 8 &&
	  SNMP_ERR_WRONGENCODING == 9 &&
	  SNMP_ERR_WRONGVALUE == 10 &&
	  SNMP_ERR_NOCREATION == 11 &&
	  SNMP_ERR_INCONSISTENTVALUE == 12 &&
	  SNMP_ERR_RESOURCEUNAVAILABLE == 13 &&
	  SNMP_ERR_COMMITFAILED == 14 &&
	  SNMP_ERR_UNDOFAILED == 15 &&
	  SNMP_ERR_AUTHORIZATIONERROR == 16 &&
	  SNMP_ERR_NOTWRITABLE == 17
	 ), 2);

netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID,
                       NETSNMP_DS_AGENT_NO_ROOT_ACCESS, 1);
my $agent = new NetSNMP::agent('Name' => 'test',
			       'Ports' => '9161');
print it($agent, 3);

$regitem = $agent->register("test_reg", ".1.3.6.1.8888", \&testsub);
print it($regitem, 4);
#print STDERR $regitem,":",ref($regitem),"\n";
print it(ref($regitem) eq "NetSNMP::agent::netsnmp_handler_registration", 5);

my $uptime1 = $agent->uptime();
my $uptime2 = $agent->uptime(666);
my $uptime3 = $agent->uptime(555, 444);
print it($uptime1 <= $uptime2 && $uptime2 <= $uptime3, 6);

exit;

while(1) {
    print netsnmp_ds_get_string(NETSNMP_DS_APPLICATION_ID, 
				NETSNMP_DS_AGENT_PORTS), "\n";
    $agent->agent_check_and_process(1);
    print netsnmp_ds_get_string(NETSNMP_DS_APPLICATION_ID, 
				NETSNMP_DS_AGENT_PORTS), "\n";
    print "got something\n";
}
exit;

$x = NetSNMP::agent::handler_registration::new("hi",\&testsub,".1.3.6.1.999");
print ((ref($x) eq "handler_registrationPtr") ? "ok 2\n" : "not ok 2\n");

print (($x->register() == 0) ? "ok 3\n" : "not ok 3\n");

my $y = NetSNMP::agent::register_mib("me",\&testsub,".1.3.6.1.8888");
while(1) {
  NetSNMP::agent::agent_check_and_process();
  print "got something\n";
}

#use Data::Dumper;
sub testsub {
    print STDERR "in perl handler sub\n";
    print STDERR "  args: ", join(", ", @_), "\n";
    #print STDERR "  dumped args: ", Dumper(@_);
    $oid= $_[3]->getOID();
    print STDERR "  request oid: ", ref($oid), " -> ", $oid, "\n";
    print STDERR "  mode: ", $_[2]->getMode(),"\n";
    $_[3]->setOID(".1.3.6.1.8888.1");
    $_[3]->setValue(2, 42);
    $_[3]->setValue(ASN_INTEGER, 42);
    print STDERR "  oid: ", $_[3]->getOID(),"\n";
    print STDERR "  ref: ", ref($_[3]),"\n";
    print STDERR "  val: ", $_[3]->getValue(),"\n";
}
