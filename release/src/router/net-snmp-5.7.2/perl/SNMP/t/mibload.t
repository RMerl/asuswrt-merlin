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
BEGIN {plan tests => 7}
use SNMP;

require "t/startagent.pl";

use vars qw($mibdir);

$SNMP::verbose = 0;

my $mib_file = 't/mib.txt';
my $junk_mib_file = 'mib.txt';

my $mibfile1;
my @mibdir;
my $mibfile2;

if ($^O =~ /win32/i) {
  $mibdir =~ s"/"\\"g;
  $mibfile1 = "$mibdir\\TCP-MIB.txt";
  @mibdir = ("$mibdir");
  $mibfile2 = "$mibdir\\IPV6-TCP-MIB.txt";
}
else {
  $mibfile1 = "$mibdir/TCP-MIB.txt";
  @mibdir = ("$mibdir");
  $mibfile2 = "$mibdir/IPV6-TCP-MIB.txt";
}

if ($^O =~ /win32/i) {
  $mibdir =~ s"/"\\"g;
}


######################################################################
# See if we can find a mib to use, return of 0 means the file wasn't
# found or isn't readable.

$res = SNMP::setMib($junk_mib_file,1);
ok(defined(!$res));
######################################################################
# Now we give the right name

$res = SNMP::setMib($mib_file,1);
ok(defined($res));
######################################################################
# See if we can find a mib to use

$res = SNMP::setMib($mib_file,0);
ok(defined($res));
######################## 4 ################################
# add a mib dir

$res = SNMP::addMibDirs($mibdir[0]);

SNMP::loadModules("IP-MIB", "IF-MIB", "IANAifType-MIB", "RFC1213-MIB");
#SNMP::unloadModules(RMON-MIB);
#etherStatsDataSource shouldn't be found.
#present only in 1271 & RMON-MIB.
#
# XXX: because we can't count on user .conf files, we should turn off
# support for them (maybe set SNMPCONFPATH at the top of this
# script?).  But for the mean time just search for a bogus node that
# will never exist.
$res = $SNMP::MIB{bogusetherStatsDataSource};

ok(!defined($res));

########################  5  ############################
# add mib file

$res1 = SNMP::addMibFiles($mibfile1);
ok(defined($res1));
$res2 = SNMP::addMibFiles($mibfile2);
ok(defined($res2));

$res = $SNMP::MIB{ipv6TcpConnState}{moduleID};

ok($res =~ /^IPV6-TCP-MIB/);
#################################################

snmptest_cleanup();

