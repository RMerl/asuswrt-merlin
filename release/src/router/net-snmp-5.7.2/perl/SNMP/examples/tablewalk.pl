# snmpwalk of a single table
# getnext of 3 columns from ipAddrEntry table
# stop after last row in table

use SNMP 1.8;

my $host = shift || localhost;
my $comm = shift || public;

my $sess = new SNMP::Session ( DestHost => $host, Community => $comm );

my $vars = new SNMP::VarList([ipAdEntAddr],[ipAdEntIfIndex],[ipAdEntNetMask]);

for (@vals = $sess->getnext($vars);
     $vars->[0]->tag =~ /ipAdEntAddr/       # still in table
     and not $sess->{ErrorStr};          # and not end of mib or other error
     @vals = $sess->getnext($vars)) {
     print "   ($vals[1]) $vals[0]/$vals[2]\n";
}
