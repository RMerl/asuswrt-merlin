#!./perl

BEGIN {
    unless(grep /blib/, @INC) {
        chdir 't' if -d 't';
        @INC = '../lib' if -d '../lib';
    }
    $ENV{'MIBS'} = '';
}

# This merely checks to see if the default_store routines work from
# read configuration files.  Functionally, if this fails then it's a
# serious problem because they linked with static libraries instead of
# shared ones as the memory space is different.

use Test;
BEGIN {plan tests => 3}

my $envsep = ($^O =~ /win32/i) ? ';' : ':';

SNMP::setenv('SNMPCONFPATH', '.' . $envsep . 't', 1);

ok(1); # just start up

use SNMP;
use NetSNMP::default_store(':all');

# should be 0, as it's un-initialized
$myint = netsnmp_ds_get_int(NETSNMP_DS_LIBRARY_ID, 
			    NETSNMP_DS_LIB_NUMERIC_TIMETICKS);

ok($myint == 0);

SNMP::init_snmp("conftest");

$myint = netsnmp_ds_get_boolean(NETSNMP_DS_LIBRARY_ID,
				NETSNMP_DS_LIB_NUMERIC_TIMETICKS);

# ok, should be 1 as it's initalized by the snmp.conf config file.
ok($myint == 1);

# this is a pretty major error, so if it's not true we really really
# print a big big warning.  Technically, I suspect this is a bad thing
# to do in perl tests but...
if ($myint != 1) {
    die "\n\n\n" . "*" x 75 . "\nBIG PROBLEM($myint): I wasn't able to read
    data from a configuration file.  This likely means that you've
    compiled the net-snmp package with static libraries, which can
    cause real problems with the perl module.  Please reconfigure your
    net-snmp package for use with shared libraries (run configure with
    --enable-shared)\n" . "*" x 75 . "\n\n\n\n";
}

