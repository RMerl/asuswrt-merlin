use SNMP;

$SNMP::auto_init_mib = 0; 

$sess = new SNMP::Session(); 

sub poller {  
   # VarList is undefined if TIMEOUT occured
   if (!defined($_[1])) { die "request timed out[$_[0]->{ErrorStr}]\n"; }
   if ($i++>100000) { die "completed 500 polls\n"; }
   #print $_[1][0]->tag, " = ", $_[1][0]->val, "\n";
   $_[0]->get($_[1], [\&poller, $_[0]]);
} 

$sess->get([[".1.3.6.1.2.1.1.3.0"]], [\&poller, $sess]); 

SNMP::MainLoop();
