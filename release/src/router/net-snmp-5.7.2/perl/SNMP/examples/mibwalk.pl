# snmpwalk of entire MIB
# stop on error at end of MIB

use SNMP 1.8;
$SNMP::use_sprint_value = 1;
my $host = shift || localhost;
my $comm = shift || public;

$sess = new SNMP::Session(DestHost => $host, Community => $comm);

$var = new SNMP::Varbind([]);

do {
  $val = $sess->getnext($var);
  print SNMP::Varbind::tag($var).".".SNMP::Varbind::iid($var)." = ".
        SNMP::Varbind::val($var)."\n";
} until ($sess->{ErrorStr});
