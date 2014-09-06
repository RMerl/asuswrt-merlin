#!./perl

BEGIN {
    unless(grep /blib/, @INC) {
        chdir 't' if -d 't';
        @INC = '../lib' if -d '../lib';
    }
    eval "use Cwd qw(abs_path)";
    $ENV{'SNMPCONFPATH'} = 'nopath';
    $ENV{'MIBDIRS'} = '+' . abs_path("../../mibs");
}
use Test;
BEGIN { $n = 17; plan tests => $n }
use SNMP;
use vars qw($agent_port $comm $agent_host);
require "t/startagent.pl";
$SNMP::debugging = 0;
$SNMP::verbose = 0;
$SNMP::dump_packet = 0;

my $junk_oid = ".1.3.6.1.2.1.1.1.1.1.1";
my $oid = '.1.3.6.1.2.1.1.1';
my $junk_name = 'fooDescr';
my $junk_host = 'no.host.here';
my $name = "gmarzot\@nortelnetworks.com";
my $s1;

# create list of varbinds for GETS, val field can be null or omitted
$vars = new SNMP::VarList (
			   ['sysDescr', '0', ''],
			   ['sysObjectID', '0'],
			   ['sysUpTime', '0'],
			   ['sysContact', '0'],
			   ['sysName', '0'],
			   ['sysLocation', '0'],
			   ['sysServices', '0'],

			   ['snmpInPkts', '0'],
			   ['snmpInBadVersions', '0'],
			   ['snmpInBadCommunityNames', '0'],
			   ['snmpInBadCommunityUses', '0'],
			   ['snmpInASNParseErrs', '0'],
			   ['snmpEnableAuthenTraps', '0'],

			   ['sysORID', '1'],
			   ['sysORDescr', '1'],
			   ['sysORUpTime', '1'],
			   ['sysORLastChange', '0'],
			   ['snmpSilentDrops', '0'],
			   ['snmpProxyDrops', '0'],

## not all agents we know will support these objects
#			   ['hrStorageType', '2'],
#			   ['hrSystemDate', '0'],
#			   ['sysORIndex', '1'],
#			   ['ifName', '1'],
#			   ['ifNumber', '0'],
#			   ['ifDescr', '1'],
#			   ['ifSpeed', '1'],
#			   ['snmpTrapEnterprise', '2'],
#			   ['ipInHdrErrors', '0'],
#			   ['ipDefaultTTL', '0'],
#			   ['ipInHdrErrors', '0'],

		          );
################################################################
# Yet to do:
# test for the max limit the 'get' can provide.
# Figure out why the IP and Physical address are not getting printed.
# why ifname is not getting printed?
################################################################
#			   ['ipNetToMediaPhysAddress', '0'],
#			   ['ipAdEntAddr', '0'],
#			   ['snmpTrapOID', '0'],
#			   ['hrSystemNumUsers', '0'],
#			   ['hrFSLastFullBackupDate', '0'],
#			   ['ifPromiscuousMode', '0'],



######################################################################
# Fire up a session.
    $s1 =
    new SNMP::Session (DestHost=>$agent_host,Version=>1,Community=>$comm,RemotePort=>$agent_port);
    ok(defined($s1));

######################################################################
# Get the standard Vars and check that we got some defined vars back
@ret = $s1->get($vars);
ok(!$s1->{ErrorStr} and defined($ret[0]));
#print STDERR "Error string = $s1->{ErrorStr}:$s1->{ErrorInd}\n";
######################################################################
# Check that we got back the number we asked for.
ok($#ret == $#{$vars});
#print("dude : $#ret\n");
################################################

# Test for a string
$contact = $s1->get('sysContact.0');
#print("contact is : $contact\n");
ok( defined($contact));


$name = $s1->get('sysName.0');
#print("Name is : $name\n");
ok( defined($name));


$location = $s1->get('sysLocation.0');
#print("Location is : $location\n");
ok( defined($location));
#########################################


# Test for an integer
$ttl = $s1->get('ipDefaultTTL.0');
#print("TTL is : $ttl\n");
ok( defined($ttl));
########################################


# Test for a TimeTicks
$time = $s1->get('sysUpTime.0');
#print("up time is : $time hundredths of a second\n");
ok( defined($time));
#########################################


#Test for a Counter32 type.
$totalDatagramsReceived = $s1->get('ipInHdrErrors.0');
#print("totalDatagramsReceived is : $totalDatagramsReceived\n");
ok( defined($totalDatagramsReceived));
################################################


#Test for a PhysicalAddr type
$physaddr = $s1->get('ipNetToMediaPhysAddress.0');
#print("physical addr is : $physaddr \n");
ok( defined($physaddr));
##############################################


#Test for a IpAddr type
$ipaddr = $s1->get('ipAdEntAddr.0');
#print("Ip address is : $ipaddr \n");
ok( defined($ipaddr));
##############################################


#Test for a OID type
$trapOID = $s1->get('snmpTrapOID.0');
#print("trap OID is : $trapOID $s1->{ErrorStr}\n");
ok( defined($trapOID));
##############################################


#Test for a Gauge type
#$numusers = $s1->get('hrSystemNumUsers.0');
#print("Number of users is : $numusers \n");
#ok( defined($numusers));
##############################################

#nosuchname
#Test for a date & time type
#$datetime = $s1->get('hrFSLastFullBackupDate.0');
#print("Number of users is : $datetime \n");
#ok( defined($datetime));
##############################################

#nosuchname
#Test for a Truth value type
#$mode = $s1->get('ifPromiscuousMode.16');
#print("Truth value(1 true, 2 false) is : $mode \n");
#ok( defined($mode));
##############################################

# Time stamp test
$time = $s1->get('sysORLastChange.0');
#print("time stamp is : $time \n");
ok(defined($time));
#############################################

# Integer test
#$index = $s1->get('sysORIndex.0');
#print("index is : $index\n");
#ok(defined($index));
#############################################

# OID test
$oid = $s1->get('sysORID.1');
#print("index is : $oid\n");
ok(defined($oid));
#############################################

# String test
$descr = $s1->get('sysORDescr.1');
#print("Sys Descr is : $descr\n");
ok(defined($descr));
#############################################

# string String test
$ifname = $s1->get('ifDescr.1');
#print("ifname is : $ifname $s1->{ErrorStr}\n");
ok(defined($ifname));
#############################################


# Try getting some unknown(wrong ?) data
$unknown = $s1->get('ifmyData.0');
ok(!defined($unknown));
##############################################


snmptest_cleanup();


