use strict;
use vars qw();
use SNMP qw();

&SNMP::initMib();

&SNMP::loadModules(
  'RFC2127-MIB',
  );

sub trap_call_setup;
sub trap_dummy;

#
# should eventually get these out of the MIB...
#
my %dispatch_table = (
  'isdnMibCallInformation', \&trap_call_setup,
  '.', \&trap_dummy,
);

sub trap_dispatcher
{
  my $session = shift;
  my $ref = shift;
  my $trapType;
  my ($reqid, $addr, $community);

  # if this is a timeout, then there will be no args...
  if (defined($ref)) {
    $ref->[1]->[2] = SNMP::translateObj($ref->[1]->val);
    $trapType = $ref->[1]->val;
    my $args = shift;
    ($reqid, $addr, $community) = @{$args};
  } else {
    $trapType = 'timeout';
  }

  if (defined($dispatch_table{$trapType})) {
    &{$dispatch_table{$trapType}}($session, $ref);
  } elsif (defined($dispatch_table{'.'})) {
    &{$dispatch_table{'.'}}($session, $ref);
  } else {
    # don't do anything... silently discard.
  }
}

sub trap_dummy
{
  my $session = shift;
  my $ref = shift;

  my $trapType = $ref->[1]->val;

  warn "unexpected trap " . $trapType;
}


sub trap_call_setup
{
  my $session = shift;
  my $varlist = shift;
  my $args = shift;

  my $ifIndex = $varlist->[2]->val;
  my $isdnBearerOperStatus = $varlist->[3]->val;
  my $isdnBearerPeerAddress = $varlist->[4]->val;
  my $isdnBearerPeerSubAddress = $varlist->[5]->val;
  my $isdnBearerInfoType = $varlist->[6]->val;
  my $isdnBearerCallOrigin = $varlist->[5]->val;

  my ($reqid, $ipaddr, $community) = @{$args};

  printf "Call from %s", $isdnBearerPeerAddress;
  printf "*%s", $isdnBearerPeerSubAddress if ($isdnBearerPeerSubAddress ne '');
  printf "\n";
}

my $session = new SNMP::Session(
  DestHost => 'udp:162',
  LocalPort => 1,
  Version => '2c',
  UseEnums => 0,
  );

if (!defined($session)) {
  die "can't create listener session";
}

# otherwise assume that ErrorNum is zero...

$session->SNMP::_catch([\&trap_dispatcher, $session]);

&SNMP::MainLoop();
