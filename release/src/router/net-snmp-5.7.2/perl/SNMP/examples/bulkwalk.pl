use SNMP;

# Hard-coded hostname and community.  This is icky, but I didn't want to 
# muddle the example with parsing command line arguments.  Deal with it. -r
#
my $hostname='localhost';
my $port='161';
my $community='public';

$SNMP::debugging = 0;
$SNMP::dump_packet = 0;

$sess = new SNMP::Session( 'DestHost'	=> $hostname,
			   'Community'	=> $community,
			   'RemotePort'	=> $port,
			   'Timeout'	=> 300000,
			   'Retries'	=> 3,
			   'Version'	=> '2c',
			   'UseLongNames' => 1,	   # Return full OID tags
			   'UseNumeric' => 1,	   # Return dotted decimal OID
			   'UseEnums'	=> 0,	   # Don't use enumerated vals
			   'UseSprintValue' => 0); # Don't pretty-print values

die "Cannot create session: ${SNMP::ErrorStr}\n" unless defined $sess;

# Set up a list of two non-repeaters and some repeated variables.
#
# IMPORTANT NOTE:
#
#   The 'get' performed for non-repeaters is a "GETNEXT" (the non-repeater
#   requests are not fulfilled with SNMP GET's).  This means that you must
#   ask for the lexicographically preceeding variable for non-repeaters.
#
#   For most branches (i.e. 'sysUpTime'), this "just works" -- be sure you
#   don't ask for an instance, and the response will be as expected.  However,
#   if you want a specific variable instance (i.e. 'ifSpeed.5'), you must 
#   ask for the _preceeding_ variable ('ifSpeed.4' in this example).
#
#   See section 4.2.3 of RFC 1905 for more details on GETBULK PDU handling.
#

my $vars = new SNMP::VarList(	['sysUpTime'],	# Nonrepeater variable
				['ifNumber'],	# Nonrepeater variable
				['ifSpeed'],	# Repeated variable
				['ifDescr'] );  # Repeated variable.

# Do the bulkwalk of the two non-repeaters, and the repeaters.  Ask for no
# more than 8 values per response packet.  If the caller already knows how
# many instances will be returned for the repeaters, it can ask only for
# that many repeaters.
#
@resp = $sess->bulkwalk(2, 8, $vars);
die "Cannot do bulkwalk: $sess->{ErrorStr} ($sess->{ErrorNum})\n"
							if $sess->{ErrorNum};

# Print out the returned response for each variable.
for $vbarr ( @resp ) {
    # Determine which OID this request queried.  This is kept in the VarList
    # reference passed to bulkwalk().
    $oid = $$vars[$i++]->tag();

    # Count the number of responses to this query.  The count will be 1 for
    # non-repeaters, 1 or more for repeaters.
    $num = scalar @$vbarr;
    print "$num responses for oid $oid: \n";

    # Display the returned list of varbinds using the SNMP::Varbind methods.
    for $v (@$vbarr) {
	printf("\t%s = %s (%s)\n", $v->name, $v->val, $v->type);
    }
    print "\n";
}

#
# Now do the same bulkwalk again, but in asynchronous mode.  Set up a Perl
# callback to receive the reference to the array of arrays of Varbind's for
# the return value, and pass along the $vars VarList to it.  This allows us
# to print the oid tags (the callback code is almost the same as above).
#
# First, define the Perl callback to be called when the bulkwalk completes.
# The call to SNMP::finish() will cause the SNMP::MainLoop() to return once
# the callback has completed, so that processing can continue.
#
sub callback {
    my ($vars, $values) = @_;
    
    for $vbarr ( @$values ) {
	# Determine which OID this request queried.  This is kept in the 
	# '$vars' VarList reference passed to the Perl callback by the
	# asynchronous callback.
	$oid = (shift @$vars)->tag();

	# Count the number of responses to this query.  The count will be 1 for
	# non-repeaters, 1 or more for repeaters.
	$num = scalar @$vbarr;
	print "$num responses for oid $oid: \n";

	# Display the returned list of varbinds using the SNMP::Varbind methods.
	for $v (@$vbarr) {
	    printf("\t%s = %s (%s)\n", $v->name, $v->val, $v->type);
	}
	print "\n";
    }

    SNMP::finish();
}

# The actual bulkwalk request is done here.  Note that the $vars VarList 
# reference will be passed to the Perl callback when the bulkwalk completes.
# 
my $reqid = $sess->bulkwalk(2, 8, $vars, [ \&callback, $vars ]);
die "Cannot do async bulkwalk: $sess->{ErrorStr} ($sess->{ErrorNum})\n"
							if $sess->{ErrorNum};

# Now drop into the SNMP event loop and await completion of the bulkwalk.
# The call to SNMP::finish() in &callback will make the SNMP::MainLoop()
# return to the caller.
#
SNMP::MainLoop();

exit 0;
