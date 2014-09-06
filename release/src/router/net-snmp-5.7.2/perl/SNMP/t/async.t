#!./perl
BEGIN {
    unless(grep /blib/, @INC) {
        chdir 't' if -d 't';
        @INC = '../lib' if -d '../lib';
    }
    eval "use Cwd qw(abs_path)";
    $ENV{'SNMPCONFPATH'} = 'nopath';
    $ENV{'MIBDIRS'} = '+' . abs_path("../../mibs");

    $skipped_tests = ($^O =~ /win32/i) ? 20 : 0;
}

use Test;
BEGIN {plan tests => 20 - $skipped_tests}
use SNMP;
use vars qw($agent_port $comm $agent_host);

if ($^O =~ /win32/i) {
  warn "Win32/Win64 detected - skipping async calls\n";
  exit;
}

require "t/startagent.pl";


sub cb1; # forward reference
sub cb2;
sub cb3;
sub cb4;
sub cb5;
sub cb6;
sub cb7;
sub cbDummy;

$SNMP::verbose = 0;
$SNMP::dump_packet = 0;

my $sess = new SNMP::Session(DestHost => $agent_host, 
			  Version => 1, 
			  Community => $comm, 
			  RemotePort => $agent_port);

# try getting unregistered OID.
my $result = $sess->get([["HooHaaHooHaa","0"]], [\&cbDummy, $sess]);
ok(!defined($result));

# this get should work
$result = $sess->get([["sysDescr","0"]], [\&cb1, $sess]);
ok($result);

SNMP::MainLoop();

snmptest_cleanup();

exit 0;

sub cb1{
    my $sess = shift;
    my $vlist = shift;

    ok(defined($vlist));
    my $tag = $vlist->[0]->tag;
    ok($tag eq 'sysDescr');
    my $val = $vlist->[0]->val;
    ok(defined $val);
    my $iid = $vlist->[0]->iid;
    my $type = $vlist->[0]->type;
    ok($type eq 'OCTETSTR');
    my $res = $sess->getnext([["sysDescr",0]], [\&cb2, $sess]);
    ok ($res);
} # end of cb1

sub cb2{
    my $sess = shift;
    my $vlist = shift;

    ok(defined($vlist));
    ok(ref($vlist->[0]) =~ /Varbind/);
    ok($vlist->[0][0] eq 'sysObjectID');
    my $res = $sess->get([[".1.3.6.1.2.1.1.1.0"]], [\&cb3, $sess]);
    ok($res);
} # end of cb2

sub cb3{
    my $sess = shift;
    my $vlist = shift;

    ok(defined($vlist));

    ok($vlist->[0][0] eq 'sysDescr');

    my $res = $sess->getnext([["sysDescr",0]], [\&cb4, $sess]);
    ok($res);
} # end of cb3

sub cb4{
    my $sess = shift;
    my $vlist = shift;

    ok(defined $vlist);
    my $res = $sess->set("sysDescr.0", "hahaha", [\&cb5, $sess]);
} # end of cb4

sub cb5{
    my $sess = shift;
    my $vlist = shift;

    ok(defined($vlist));

    my $res = $sess->set("sysORID.1", ".1.3.6.1.2.1.1.1", [\&cb6, $sess]);
    ok(defined $res);
} # end of cb5

sub cb6{
    my $sess = shift;
    my $vlist = shift;
    my $tag = $vlist->[0]->tag;
    my $val = $vlist->[0]->val;

    ok($tag =~ /^sysORID/);
# create list of varbinds for GETS, val field can be null or omitted
    my $vars =
	new SNMP::VarList (
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
			   ['sysORID', '1'],
			   ['sysORDescr', '1'],
			   ['sysORUpTime', '1'],
			   ['sysORLastChange', '0'],
			   ['ipInHdrErrors', '0'],
			   ['ipDefaultTTL', '0'],
			   ['ipInHdrErrors', '0'],
			   );
    my $res = $sess->get($vars, [\&cb7, $sess]);
    ok(defined $res);
} # end of cb6

sub cb7{
    my $sess = shift;
    my $vlist = shift;


    my $tag = $vlist->[0]->tag;
    my $val = $vlist->[0]->val;

    ok(@{$vlist} == 23);

    SNMP::finish();
} # end of cb7

sub cbDummy {
    warn("error: this should not get called");
}

