#!/usr/bin/perl

# HEADER Basic perl functionality to a UDP agent

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

my $value;

plan(tests => 10);

ok(1,1,"started up");

# use a basic UDP port
my $destination = "udp:localhost:9897";

my $test = new NetSNMPTest(agentaddress => $destination);

# set it up with a snmpv3 USM user
$test->config_agent("createuser testuser MD5 notareallpassword");
$test->config_agent("rwuser testuser");
$test->config_agent("syscontact itworked");

$test->DIE("failed to start the agent") if (!$test->start_agent());

# now create a session to test things with
my $session = new SNMP::Session(DestHost => $destination,
                                Version => '3',
				SecName => 'testuser',
				SecLevel => 'authNoPriv',
				AuthProto => 'MD5',
				AuthPass => 'notareallpassword');

ok(ref($session), 'SNMP::Session', "created a session");


######################################################################
# GET test
$value = $session->get('sysContact.0');

ok($value, 'itworked');

######################################################################
# GETNEXT test
$value = $session->getnext('sysContact');

ok($value, 'itworked');

######################################################################
# SET test
$value = $session->get('sysLocation.0');

ok($value ne 'yep', 1, 'Ensuring the sysLocation setting is not "yep"');

my $varbind = new SNMP::Varbind(['sysLocation', '0', 'yep', 'OCTETSTR']);


$value = $session->set($varbind);

ok(($value == 0), 1, 'return value from set was a success');

my $value = $session->get('sysLocation.0');

ok($value, 'yep');

######################################################################
# GETBULK test
$varbind = new SNMP::Varbind(['sysContact']);
my @values = $session->getbulk(0, 3, $varbind);

ok($#values == 2);
ok($values[0] eq 'itworked');
ok($values[2] eq 'yep');

######################################################################
# gettable() test



######################################################################
# cleanup
$test->stop_agent();
