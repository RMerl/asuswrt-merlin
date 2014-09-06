#!./perl

# Written by John Stoffel (jfs@fluent.com) - 10/13/1997

BEGIN {
    unless(grep /blib/, @INC) {
        chdir 't' if -d 't';
        @INC = '../lib' if -d '../lib';
    }
    eval "use Cwd qw(abs_path)";
    $ENV{'SNMPCONFPATH'} = 'nopath';
    $ENV{'MIBDIRS'} = '+' . abs_path("../../mibs");
    $ENV{'MIBS'} = 'ALL';
}

# to print the description...
$SNMP::save_descriptions = 1;

use Test;
BEGIN {plan tests => 35}
use SNMP;

$SNMP::verbose = 0;
$SNMP::best_guess = 2;

use vars qw($bad_oid);
require "t/startagent.pl";

#############################  1  ######################################
#check if
my $res = $SNMP::MIB{sysDescr}{label};
#print("Label is:$res\n");
ok("sysDescr" eq $res);
#print("\n");
#############################  2  ######################################
$res =  $SNMP::MIB{sysDescr}{objectID};
#print("OID is: $res\n");
ok(defined($res));
#print("\n");
#############################  3  ######################################
$res =  $SNMP::MIB{sysDescr}{access};
#print("access is: $res\n");
ok($res eq 'ReadOnly');
#print("\n");
##############################  4  ###################################
$res =  $SNMP::MIB{sysLocation}{access};
#$res =  $SNMP::MIB{sysORIndex}{access};
ok($res eq 'ReadWrite');
##############################  5  ###################################
$res =  $SNMP::MIB{sysLocation}{type};
ok($res eq 'OCTETSTR');
#############################  6  ####################################
$res =  $SNMP::MIB{sysLocation}{status};
#print STDERR ("status is: $res\n");
ok($res eq 'Current');
#print STDERR ("\n");
#############################  7  #################################
$res =  $SNMP::MIB{sysORTable}{access};
#print("access is: $res\n");
ok($res eq 'NoAccess');
#print("\n");
#############################  8  ###############################
$res = $SNMP::MIB{sysLocation}{subID};
#print("subID is: $res\n");
ok(defined($res));
#print("\n");
############################  9  ##############################
$res = $SNMP::MIB{sysLocation}{syntax};
#print("syntax is: $res\n");
ok($res eq 'DisplayString');
#print("\n");
############################  10  ###########################
$res = $SNMP::MIB{ipAdEntAddr}{syntax};
ok($res eq 'IPADDR');
#print("\n");
##########################  11  ##########################
$res = $SNMP::MIB{atNetAddress}{syntax};
#print ("syntax is: $res\n");
ok($res eq 'NETADDR');
#print("\n");
########################   12  ###############################
$res = $SNMP::MIB{ipReasmOKs}{syntax};
#print("syntax is: $res\n");
ok($res eq 'COUNTER');
#print("\n");
######################   13  ##############################
$res = $SNMP::MIB{sysDescr}{moduleID};
#print("Module ID is: $res\n");
ok(defined($res));
#print("\n");
######################  14   #########################
$des = $SNMP::MIB{atNetAddress}{description};
#print("des is --> $des\n");
ok(defined($des));
#print("\n");

######################  15   #########################
$res = $SNMP::MIB{atNetAddress}{nextNode};
#print("res is --> $res\n");
ok(ref($res) eq "HASH");
#print("\n");

########################  16   #########################
$res = $SNMP::MIB{sysDescr}{children};
#print("res is --> $res\n");
ok(ref($res) eq "ARRAY");
#print("\n");
####################  17   #########################

$res = $SNMP::MIB{sysDescr}{badField};
ok(!defined($res));


######################  18   #########################
$res = $SNMP::MIB{sysDescr}{hint};
#print("res is --> $res\n");
#XXX: test fails due SMIv1 codes being returned intstead of SMIv2...
#ok(defined($res) && $res =~ /^255a/);
#print("\n");
######################  19   #########################

$res = $SNMP::MIB{ifPhysAddress}{hint};
#print("res is --> $res\n");
#XXX: test fails due SMIv1 codes being returned intstead of SMIv2...
#ok(defined($res) && $res =~ /^1x:/);
#print("\n");


######################  some translate tests  #######

#####################  20  #########################
# Garbage names return Undef.

my $type1 = SNMP::getType($bad_name);
ok(!defined($type1));
#printf "%s %d\n", (!defined($type1)) ? "ok" :"not ok", $n++;

######################################################################
# getType() supports numeric OIDs now

my $type2 = SNMP::getType($oid);
#XXX: test fails due SMIv1 codes being returned intstead of SMIv2...
#ok(defined($type2) && $type2 =~ /OCTETSTR/);

######################################################################
# This tests that sysDescr returns a valid type.

my $type3 = SNMP::getType($name);
ok(defined($type3));

######################################################################
# Translation tests from Name -> OID
# sysDescr to .1.3.6.1.2.1.1.1
$oid_tag = SNMP::translateObj($name);
ok($oid eq $oid_tag);

######################################################################
# Translation tests from Name -> OID
# RFC1213-MIB::sysDescr to .1.3.6.1.2.1.1.1
$oid_tag = SNMP::translateObj($name_module);
ok($oid eq $oid_tag);

######################################################################
# Translation tests from Name -> OID
# .iso.org.dod.internet.mgmt.mib-2.system.sysDescr to .1.3.6.1.2.1.1.1
$oid_tag = SNMP::translateObj($name_long);
ok($oid eq $oid_tag);

######################################################################
# bad name returns 'undef'
$oid_tag = '';
$oid_tag = SNMP::translateObj($bad_name);
ok(!defined($oid_tag));


######################################################################
# OID -> name
# .1.3.6.1.2.1.1.1 to sysDescr
$name_tag = SNMP::translateObj($oid);
ok($name eq $name_tag);

######################################################################
# OID -> name
# .1.3.6.1.2.1.1.1 to RFC1213-MIB::sysDescr or
# .1.3.6.1.2.1.1.1 to SNMPv2-MIB::sysDescr
$name_tag = SNMP::translateObj($oid,0,1);
$name_module2 = $name_module2; # To eliminate 'only use once' variable warning
ok(($name_module eq $name_tag) || ($name_module2 eq $name_tag));

######################################################################
# OID -> name
# .1.3.6.1.2.1.1.1 to .iso.org.dod.internet.mgmt.mib-2.system.sysDescr
$name_tag = SNMP::translateObj($oid,1);
ok($name_long eq $name_tag);

######################################################################
# OID -> name
# .1.3.6.1.2.1.1.1 to RFC1213-MIB::.iso.org.dod.internet.mgmt.mib-2.system.sysDescr or
# .1.3.6.1.2.1.1.1 to SNMPv2-MIB::.iso.org.dod.internet.mgmt.mib-2.system.sysDescr
$name_module_long = $name_module_long; # To eliminate 'only use once' variable warning
$name_module_long2 = $name_module_long2; # To eliminate 'only use once' variable warning
$name_tag = SNMP::translateObj($oid,1,1);
ok(($name_module_long eq $name_tag) || ($name_module_long2 eq $name_tag));

######################################################################
# bad OID -> Name

$name_tag = SNMP::translateObj($bad_oid);
ok($name ne $name_tag);
#printf "%s %d\n", ($name ne $name_tag) ? "ok" :"not ok", $n++;


######################################################################
# ranges

$node = $SNMP::MIB{snmpTargetAddrMMS};
ok($node);
$ranges = $node->{ranges};
ok($ranges and ref $ranges eq 'ARRAY');
ok(@$ranges == 2);
ok($$ranges[0]{low} == 0);
ok($$ranges[0]{high} == 0);
ok($$ranges[1]{low} == 484);
ok($$ranges[1]{high} == 2147483647);

snmptest_cleanup();
