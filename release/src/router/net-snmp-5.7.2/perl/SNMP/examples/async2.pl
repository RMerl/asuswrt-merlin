use SNMP;

$SNMP::auto_init_mib = 0; 

$sess = new SNMP::Session(); 

sub poller_handler {  
   if (++$i>500) { die "completed 500 polls\n"; };
   # VarList is undefined if TIMEOUT occured
   if (!defined($_[1])) { 
       warn "request timed out[$_[0]->{ErrorStr}]\n";
       return;
   }
#   print "$i) ",$_[1][0]->tag, " = ", $_[1][0]->val, "\n";
} 

# $sess->get([[".1.3.6.1.2.1.1.3.0"]], [\&poller_handler, $sess]); 

SNMP::MainLoop(.1,sub {for (1..50) {$sess->get([['.1.3.6.1.2.1.1.3.0']], [\&poller_handler, $sess]);} });
