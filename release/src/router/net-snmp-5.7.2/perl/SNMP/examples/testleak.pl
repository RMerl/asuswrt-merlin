use SNMP 1.6;

$host = shift;
unless ($host) {
  $| = 1;  print "enter SNMP host address: "; $| = 0;
  chomp($host = <STDIN>);
}

$obj = new SNMP::Session DestHost, $host;

while (){
print $obj->get(["ifNumber",0]);
  open(COM,"ps -u$$|") || die;
  @bar = <COM>; 
  $siz = (split(' ',$bar[1]))[4];
  $rss = (split(' ',$bar[1]))[5];
  close(COM);
  print "siz = $siz, rss = $rss\n";
}
