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
BEGIN { plan tests => 7 }
use SNMP;
use vars qw($agent_port $comm $agent_host);
require "t/startagent.pl";


my $junk_oid = ".1.3.6.1.2.1.1.1.1.1.1";
my $oid = ".1.3.6.1.2.1.1.1";
my $junk_name = 'fooDescr';
my $junk_host = 'no.host.here';
my $name = "gmarzot\@nortelnetworks.com";

$SNMP::debugging = 0;
$n = 15;  # Number of tests to run

#print "1..$n\n";
if ($n == 0) { exit 0; }

# create list of varbinds for GETS, val field can be null or omitted
my $vars = new SNMP::VarList (
			   ['sysDescr', '0', ''],
			   ['sysObjectID', '0'],
			   ['sysUpTime', '0'],
			   ['sysContact', '0'],
			   ['sysName', '0'],
			   ['sysLocation', '0'],
			   ['sysServices', '0'],
			   ['ifNumber', '0'],
			   ['ifDescr', '1'],
			   ['ifSpeed', '1'],

			   ['snmpInPkts', '0'],
			   ['snmpInBadVersions', '0'],
			   ['snmpInBadCommunityNames', '0'],
			   ['snmpInBadCommunityUses', '0'],
			   ['snmpInASNParseErrs', '0'],
			   ['snmpEnableAuthenTraps', '0'],
#			   ['snmpSilentDrops', '0'],
#			   ['snmpProxyDrops', '0'],
#			   ['snmpTrapEnterprise', '2'],

#			   ['hrStorageType', '2'],
#			   ['hrSystemDate', '0'],
			   ['sysORIndex', '1'],
			   ['sysORID', '2'],
			   ['sysORDescr', '3'],
			   ['sysORUpTime', '4'],
#			   ['ifName', '1'],
			   ['sysORLastChange', '0'],
			   ['ipInHdrErrors', '0'],
			   ['ipDefaultTTL', '0'],
			   ['ipInHdrErrors', '0'],
		          );
################################################################
#			   ['ipNetToMediaPhysAddress', '0'],
#			   ['ipAdEntAddr', '0'],
#			   ['snmpTrapOID', '0'],
#			   ['hrSystemNumUsers', '0'],
#			   ['hrFSLastFullBackupDate', '0'],
#			   ['ifPromiscuousMode', '0'],



#########################  1  #######################################
# Fire up a session.
    my $s1 =
    new SNMP::Session (DestHost=>$agent_host,Version=>1,Community=>$comm,RemotePort=>$agent_port);
    ok(defined($s1));

#######################  2  ##########################################
# Set some value and see if the value is set properly.

$originalLocation = $s1->get('sysLocation.0');
$value = 'Router Management Labs';
$s1->set('sysLocation.0', $value);
$finalvalue = $s1->get('sysLocation.0');
ok($originalLocation ne $finalvalue);
#print STDERR "Error string = $s1->{ErrorStr}:$s1->{ErrorInd}\n";
#print("set value is: $finalvalue\n\n");
$s1->set('sysLocation.0', $originalLocation);

########################   3   #######################################

# Now, reset that string with a non-string value.
# This will FAIL. :)

#$nonstrvalue = '.9.23.56.7';
#$s1->set('sysLocation.0', $nonstrvalue);
#$finalvalue = $s1->get('sysLocation.0');
#ok(!defined($finalvalue));

#if (($initialvalue cmp $finalvalue) != 0 ) {
#    ok(1);
#}
#print STDERR "Error string = $s1->{ErrorStr}:$s1->{ErrorInd}\n";
#print("set value is: $finalvalue\n\n");
#$s1->set('sysLocation.0', $originalLocation);

#######################   4   #####################################

# Test for an integer (READ-ONLY)
$originalservice = $s1->get('sysServices.0');
#print("services is: $originalservice\n");
$junk_service = "Nortel Networks";
$s1->set('sysServices.0', $junk_service);

$finalvalue = $s1->get('sysServices.0');
#print("services is: $finalvalue\n");
#print("Services is: $originalservice\n");
ok($originalservice eq $finalvalue);
#print STDERR "Error string = $s1->{ErrorStr}:$s1->{ErrorInd}\n";
$s1->set('sysServices.0',$originalservice);
#print("\n");

##################   5   ######################
# Test for an integer (READ-WRITE)
# The snmpEnableAuthenTraps takes only two values - 1 and 2.
# If any other value is tried to be set, it doesn't set and
# retains the old value.

$originalTrap = $s1->get('snmpEnableAuthenTraps.0');
#print("trap is -- $originalTrap\n");
$junk_trap = "Nortel Networks";
$s1->set('snmpEnableAuthenTraps.0', $junk_trap);
$finalvalue = $s1->get('snmpEnableAuthenTraps.0');
#print("final trap is: $finalvalue\n");
ok($finalvalue ne $junk_trap);
# Should the error be 'Value out of range: SNMPERR_RANGE ?
#print STDERR "Error string = $s1->{ErrorStr}:$s1->{ErrorInd}\n";
$s1->set('snmpEnableAuthenTraps.0',$originalTrap);
#print("\n");
###################  6  #######################
# Test for a TimeTicks (is this advisable? )
# Trying to set uptime which cannot be done (READ-ONLY).
#$time = $s1->get('sysUpTime.0');
#print("up time is : $time hundredths of a second\n");
#$junk_time = 12345;
#$s1->set('sysUpTime.0', $junk_time);
#$finalvalue = $s1->get('sysUpTime.0');
#print("final time is: $finalvalue hundredths of a second \n");
# Will the final value always be equal to the initial value?
# depends on how fast this piece of code executes?
#ok($finalvalue == $time);
#print STDERR "Error string = $s1->{ErrorStr}:$s1->{ErrorInd}\n";
#print("\n");

###################   7   ######################


#Test for a Counter32 type.
# READ-ONLY.

#$Pkts = $s1->get('snmpInPkts.0');
#print(" pkts is : $Pkts\n");
#$junk_pkts = -1234;
#$s1->set('snmpInPkts.0', $junk_pkts);
#$finalPkts = $s1->get('snmpInPkts.0');
#print("now pkts is : $finalPkts\n");
#ok($finalPkts > $Pkts);
# Expecting genErr
#ok($s1->{ErrorStr} =~ /^\(gen/);
#print STDERR "pkts is = $s1->{ErrorStr}:$s1->{ErrorInd}\n";
#print("\n");
##################   8   ##############################

# Set a non-accessible attribute
$s1->set('ipAddrEntry.1', 'MyEID');
# What should I expect - genErr or Bad variable type ?
# What gets checked first - type or accessibility?
# if type, then this is right..else, genErr is expected.
ok($s1->{ErrorStr} =~ /^Bad/ );
#print STDERR "Error string = $s1->{ErrorStr}:$s1->{ErrorInd}\n";
#print("\n");

#################  12  ##########################
# Time stamp test - READ-ONLY
#$origtime = $s1->get('sysORLastChange.0');
#print("Time is: $origtime\n");
#print STDERR "Error string = $s1->{ErrorStr}:$s1->{ErrorInd}\n";
#$time = $s1->set('sysORLastChange.0', 12345);
#print("time stamp is : $time \n");
# Should get genErr.
#ok($time =~ /^genErr/);
#print STDERR "Error string = $s1->{ErrorStr}:$s1->{ErrorInd}\n";
#print("\n");

##############   13   ############################

# OID test
my $oldoid = $s1->get("sysORID.1");
#print("OID is : $oldoid\n");
$junk_OID = ".6.6.6.6.6.6";
$s1->set('sysORID.1', $junk_OID);
$newOID = $s1->get("sysORID.1");
#print("new oid is $newOID\n");
ok($oldoid eq $newOID);
#print STDERR "Error string = $s1->{ErrorStr}:$s1->{ErrorInd}\n";
#print("\n");
################  14  ##########################

# Try setting an unregistered OID.
$junk_data = 'hehehe';
$s1->set('ifmyData.0', $junk_data);

#print STDERR "Error string = $s1->{ErrorStr}:$s1->{ErrorInd}\n";
ok( $s1->{ErrorStr} =~ /^Unknown/ );

##############################################

snmptest_cleanup();


