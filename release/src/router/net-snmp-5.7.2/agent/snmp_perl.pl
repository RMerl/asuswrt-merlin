##
## SNMPD perl initialization file.
##

use NetSNMP::agent;
$agent = new NetSNMP::agent('dont_init_agent' => 1,
			    'dont_init_lib' => 1);

