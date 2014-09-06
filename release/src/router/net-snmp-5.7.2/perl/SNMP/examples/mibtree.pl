use SNMP;
$SNMP::save_descriptions = 1; # must be set prior to mib initialization
SNMP::initMib(); # parses default list of Mib modules from default dirs

# read dotted decimal oid or symbolic name to look up
# partial name will be searched and all matches returned
$val = shift || die "supply partial or complete object name or identifier\n";

if ($node = $SNMP::MIB{$val}) {
    print "$node:$node->{label} [$node->{objectID}]\n";
    while (($k,$v) = each %$node) {
	print "\t$k => $v\n";
    }
} else {
    while (($k,$v) = each %SNMP::MIB) {
	print "$v->{label} [$v->{obj}]\n" #accepts unique partial key(objectID)
	    if $k =~ /$val/ or $v->{label} =~ /$val/;
    }
}

