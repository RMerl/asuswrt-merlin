use SNMP;
$SNMP::use_enums = 1;

my $host = shift;
my $comm = shift;
$sess = new SNMP::Session(DestHost => $host, Community => $comm);

$vars = new SNMP::VarList( ['ipRouteIfIndex'], ['ipRouteType'],
                           ['ipRouteProto'], ['ipRouteMask'],
                           ['ipRouteNextHop'], ['ipRouteAge'],
			   ['ipRouteMetric1']);

format STDOUT_TOP =
  Destination      Next Hop          Mask       Proto    Age    Metric
--------------- --------------- -------------- ------- -------- ------
.

format STDOUT =
@<<<<<<<<<<<<<< @<<<<<<<<<<<<<< @<<<<<<<<<<<<< @|||||| @||||||| @|||||
$dest,          $nhop,          $mask,         $proto, $age,    $metric
.

for (($index,$type,$proto,$mask,$nhop,$age,$metric) = $sess->getnext($vars);
     $$vars[0]->tag eq 'ipRouteIfIndex' and not $sess->{ErrorStr};
     ($index,$type,$proto,$mask,$nhop,$age,$metric) = $sess->getnext($vars)) {
    $dest = $$vars[0]->iid;
    write;
}

print "$sess->{ErrorStr}\n";
