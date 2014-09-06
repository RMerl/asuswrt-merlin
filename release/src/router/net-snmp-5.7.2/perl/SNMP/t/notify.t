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
BEGIN { $n = 11; plan tests => $n }
use SNMP;
use vars qw($agent_port $comm $comm2 $trap_port $agent_host $sec_name $priv_pass $auth_pass $bad_name);
require 't/startagent.pl';
$SNMP::debugging = 0;

my $res;
my $enterprise = '.1.3.6.1.2.1.1.1.0';
my $generic = 'specific';

#                         V1 trap testing
########################  1  ############################
# Fire up a trap session.
my $s1 =
    new SNMP::Session (DestHost=>$agent_host,Version=>1,Community=>$comm,RemotePort=>$trap_port);
ok(defined($s1));

########################  2  ############################
# test v1 trap
if (defined($s1)) {
  $res = $s1->trap(enterprise => $enterprise, agent=>$agent_host, generic=>$generic,[[sysContact, 0, 'root@localhost'], [sysLocation, 0, 'here']] );
}
ok($res =~ /^0 but true/);

########################  3  ############################
# test with wrong varbind
undef $res;
if (defined($s1)) {
  $res = $s1->trap([[$bad_name, 0, 'root@localhost'], [sysLocation, 0, 'here']] );
  #print("res is $res\n");
}
ok(!defined($res));
#########################################################

#                      V2 testing
########################  4  ############################
# Fire up a v2 trap session.
my $s2 =
    new SNMP::Session (Version=>2, DestHost=>$agent_host,Community=>$comm2,RemotePort=>$trap_port);
ok(defined($s2));
########################  5  ############################
# test v2 trap
undef $res;
if (defined($s2)) {
  $res = $s2->trap(uptime=>200, trapoid=>'coldStart',[[sysContact, 0, 'root@localhost'], [sysLocation, 0, 'here']] );
  #print("res is $res\n");
}
ok($res =~ /^0 but true/);
########################  6  ############################
# no trapoid and uptime given. Should take defaults...
my $ret;
if (defined($s2)) {
  $ret = $s2->trap([[sysContact, 0, 'root@localhost'], [sysLocation, 0, 'here']] );
  #print("res is $ret\n");
}
ok(defined($ret));
########################  7  ############################
# no varbind list given.
undef $res;
if (defined($s2)) {
  $res = $s2->trap(trapoid=>'coldStart');
  #print("res is $res\n");
}
ok(defined($res) && $res =~ /^0 but true/);

#########################################################

#                     v3 testing
########################  8  ############################
# Fire up a v3 trap session.
my $s3 = new SNMP::Session(Version=>3, DestHost=> $agent_host, RemotePort=>$trap_port, SecName => $sec_name);
ok(defined($s3));

########################  9  ############################
if (defined($s3)) {
  $res = $s3->inform(uptime=>111, trapoid=>'coldStart', [[sysContact, 0, 'root@localhost'], [sysLocation, 0, 'here']] );
}
ok($res =~ /^0 but true/);

######################## 10  ############################
# Fire up a v3 trap session.
$s3 = new SNMP::Session(Version=>3, DestHost=> $agent_host, RemotePort=>$trap_port, SecName => $sec_name, SecLevel => authPriv, AuthPass => $auth_pass, PrivPass => $priv_pass);
ok(defined($s3));

######################## 11  ############################
undef $res;
if (defined($s3)) {
    $res = $s3->inform(uptime=>111, trapoid=>'coldStart', [[sysContact, 0, 'root@localhost'], [sysLocation, 0, 'here']] );
    print "res = $res\n";
}
  
ok(defined($res) && ($res =~ /^0 but true/));

snmptest_cleanup();
