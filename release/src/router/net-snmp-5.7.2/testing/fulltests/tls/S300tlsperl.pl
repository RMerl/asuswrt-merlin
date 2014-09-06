#!/usr/bin/perl

BEGIN {
    if (exists($ENV{'srcdir'})) {
	push @INC, "$ENV{'srcdir'}/testing/fulltests/support";
    } elsif (-d "fulltests/support") {
	push @INC, "fulltests/support";
    } elsif (-d "../support") {
	push @INC, "../support";
    }
}
use NetSNMPTest;
use Test;
use SNMP;

my $test = new NetSNMPTest(agentaddress => $agentaddress);

$test->require_feature($feature);

my $netsnmpcert = "$ENV{'srcdir'}/local/net-snmp-cert -I -C $test->{'dir'}";

system("$netsnmpcert gencert -I -t snmpd > /dev/null 2>&1");
system("$netsnmpcert gencert -I -t snmpapp --cn snmpapp > /dev/null 2>&1");
system("$netsnmpcert gencert -I -t perl --cn perl > /dev/null 2>&1");

my $fp = `$netsnmpcert showcerts --fingerprint --brief perl`;
chomp($fp);
print "# using local fp: $fp\n";

plan(tests => 2);
$test->config_agent("syscontact itworked");
$test->config_agent("rwuser -s tsm perl");
$test->config_agent("certSecName 100 snmpapp --cn");
$test->config_agent("certSecName 101 $fp --cn");

$test->DIE("failed to start the agent") if (!$test->start_agent(" -Dtls,ssl,tsm,cert:map"));

# now create a session to test things with
my $session = new SNMP::Session(DestHost => $test->{'agentaddress'},
				TheirIdentity => 'snmpd',
			        OurIdentity => 'perl');

ok(ref($session), 'SNMP::Session', "created a session");

######################################################################
# GET test
if (ref($session) eq 'SNMP::Session') {
    my $value = $session->get('sysContact.0');
    ok($value, 'itworked');
}

######################################################################
# cleanup
$test->stop_agent();
