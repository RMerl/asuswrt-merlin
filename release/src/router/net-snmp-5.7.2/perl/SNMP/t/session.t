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
BEGIN { plan tests => 5}
use SNMP;
use vars qw($agent_port $comm $agent_host $bad_auth_pass $auth_pass $sec_name $bad_sec_name $bad_version $bad_priv_pass $priv_pass);
require "t/startagent.pl";

$SNMP::debugging = 0;

# create list of varbinds for GETS, val field can be null or omitted
my $vars = new SNMP::VarList (
			   ['sysDescr', '0', ''],
			   ['sysContact', '0'],
			   ['sysName', '0'],
			   ['sysLocation', '0'],
			   ['sysServices', '0'],
			   ['ifNumber', '0'],
			   ['ifDescr', '1'],
			   ['ifSpeed', '1'],
			  );

#########################== 1 ===#########################################
# Create a bogus session, undef means the host can't be found.
# removed! this test can hang for a long time if DNS is not functioning
# my $s1 = new SNMP::Session (DestHost => $bad_host );
# ok(!defined($s1));
#print("\n");
#####################== 2 ====############################################
# Fire up a session.
    my $s2 =
    new SNMP::Session (DestHost=>$agent_host, Community=>$comm,
		       RemotePort=>$agent_port);
    ok(defined($s2));
######################==  3 ==== ##########################################

# Fire up a V3 session 
my $s3 = new SNMP::Session (Version => 3 , RemotePort => $agent_port, 
			    SecName => $sec_name );
ok(defined($s3));
#print STDERR "Error string1 = $s3->{ErrorStr}:$s3->{ErrorInd}\n";
#print("\n");
#####################=== 4 ====###########################################
#create a V3 session by setting an IP address/port not running an agent
my $s4 = new SNMP::Session (Version => 3, RemotePort => 1002, Retries => 0);
# engineId discovery should fail resulting in session creation failure (undef)
ok(!defined($s4));
#print STDERR "Error string1 = $s4->{ErrorStr}:$s4->{ErrorInd}\n";
#print("\n");
######################  5  ###########################################
#create a session with bad version
my $s5 = new SNMP::Session (Version=>$bad_version);
ok(!defined($s5));
#print("\n");
########################  6  ########################################
#Test for v3 session creation success
my $s6 = new SNMP::Session (Version => 3, RemotePort => $agent_port,
			    SecLevel => 'authPriv', 
			    SecName => $sec_name, 
			    PrivPass => $priv_pass, 
			    AuthPass => $auth_pass);
ok(defined($s6));
#print STDERR "Error string2 = $s6->{ErrorStr}:$s6->{ErrorInd}\n";
#print("\n");
#####################  7  ############################################

snmptest_cleanup();
