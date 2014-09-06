package NetSNMP::agent;

use strict;
use warnings;
use Carp;

require Exporter;
require DynaLoader;
use AutoLoader;

use NetSNMP::default_store (':all');
use NetSNMP::agent::default_store (':all');
use NetSNMP::OID (':all');
use NetSNMP::agent::netsnmp_request_infoPtr;

use vars qw(@ISA %EXPORT_TAGS @EXPORT_OK @EXPORT $VERSION $AUTOLOAD);

@ISA = qw(Exporter AutoLoader DynaLoader);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use NetSNMP::agent ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
%EXPORT_TAGS = ( 'all' => [ qw(
	MODE_GET
	MODE_GETBULK
	MODE_GETNEXT
	MODE_SET_ACTION
	MODE_SET_BEGIN
	MODE_SET_COMMIT
	MODE_SET_FREE
	MODE_SET_RESERVE1
	MODE_SET_RESERVE2
	MODE_SET_UNDO
	SNMP_ERR_NOERROR
	SNMP_ERR_TOOBIG
	SNMP_ERR_NOSUCHNAME
	SNMP_ERR_BADVALUE
	SNMP_ERR_READONLY
	SNMP_ERR_GENERR
	SNMP_ERR_NOACCESS
	SNMP_ERR_WRONGTYPE
	SNMP_ERR_WRONGLENGTH
	SNMP_ERR_WRONGENCODING
	SNMP_ERR_WRONGVALUE
	SNMP_ERR_NOCREATION
	SNMP_ERR_INCONSISTENTVALUE
	SNMP_ERR_RESOURCEUNAVAILABLE
	SNMP_ERR_COMMITFAILED
	SNMP_ERR_UNDOFAILED
	SNMP_ERR_AUTHORIZATIONERROR
	SNMP_ERR_NOTWRITABLE
) ] );

@EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

@EXPORT = qw(
	MODE_GET
	MODE_GETBULK
	MODE_GETNEXT
	MODE_SET_ACTION
	MODE_SET_BEGIN
	MODE_SET_COMMIT
	MODE_SET_FREE
	MODE_SET_RESERVE1
	MODE_SET_RESERVE2
	MODE_SET_UNDO
	SNMP_ERR_NOERROR
	SNMP_ERR_TOOBIG
	SNMP_ERR_NOSUCHNAME
	SNMP_ERR_BADVALUE
	SNMP_ERR_READONLY
	SNMP_ERR_GENERR
	SNMP_ERR_NOACCESS
	SNMP_ERR_WRONGTYPE
	SNMP_ERR_WRONGLENGTH
	SNMP_ERR_WRONGENCODING
	SNMP_ERR_WRONGVALUE
	SNMP_ERR_NOCREATION
	SNMP_ERR_INCONSISTENTVALUE
	SNMP_ERR_RESOURCEUNAVAILABLE
	SNMP_ERR_COMMITFAILED
	SNMP_ERR_UNDOFAILED
	SNMP_ERR_AUTHORIZATIONERROR
	SNMP_ERR_NOTWRITABLE
);
$VERSION = '5.0702';

sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.  If a constant is not found then control is passed
    # to the AUTOLOAD in AutoLoader.

    my $constname;
    ($constname = $AUTOLOAD) =~ s/.*:://;
    croak "& not defined" if $constname eq 'constant';
    my $val;
    ($!, $val) = constant($constname);
    if ($! != 0) {
	if ($! =~ /Invalid/ || $!{EINVAL}) {
	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
	    goto &AutoLoader::AUTOLOAD;
	}
	else {
	    croak "Your vendor has not defined NetSNMP::agent macro $constname";
	}
    }
    {
	no strict 'refs';
	# Fixed between 5.005_53 and 5.005_61
#	if ($] >= 5.00561) {
#	    *$AUTOLOAD = sub () { $val };
#	}
#	else {
	    *$AUTOLOAD = sub { $val };
#	}
    }
    goto &$AUTOLOAD;
}

{
    my $haveinit = 0;

    sub mark_init_agent_done {
	$haveinit = 1;
    }

    sub maybe_init_agent {
	return if ($haveinit);
	$haveinit = 1;

	my $flags = $_[0];
	if ($flags->{'AgentX'}) {
	    netsnmp_ds_set_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_ROLE, 1);
	}
	init_agent($flags->{'Name'} || "perl");
	if ($flags->{'Ports'}) {
	    netsnmp_ds_set_string(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_PORTS, $flags->{'Ports'});
	}
	init_mib();
    }
}

{
    my $haveinit = 0;

    sub mark_init_lib_done {
	$haveinit = 1;
    }

    sub maybe_init_lib {
	return if ($haveinit);
	$haveinit = 1;

	my $flags = $_[0];
	init_snmp($flags->{'Name'} || "perl");
	if (netsnmp_ds_get_boolean(NETSNMP_DS_APPLICATION_ID, NETSNMP_DS_AGENT_ROLE) != 1) {
	    init_master_agent();
	}
    }
}

sub new {
    my $type = shift;
    my ($self);
    %$self = @_;
    bless($self, $type);
    if ($self->{'dont_init_agent'}) {
	$self->mark_init_agent_done();
    } else {
	$self->maybe_init_agent();
    }
    if ($self->{'dont_init_lib'}) {
	$self->mark_init_lib_done();
    }
    return $self;
}

sub register($$$$) {
    my ($self, $name, $oid, $sub) = @_;
    my $reg = NetSNMP::agent::netsnmp_handler_registration::new($name, $oid, $sub);
    $reg->register() if ($reg);
    return $reg;
}

sub main_loop {
    my $self = shift;
    while(1) {
	$self->agent_check_and_process(1);
    }
}

sub agent_check_and_process {
    my ($self, $blocking) = @_;
    $self->maybe_init_lib();
    __agent_check_and_process($blocking || 0);
}

sub uptime {
    my $self = shift;
    $self->maybe_init_lib();
    return _uptime();
}

bootstrap NetSNMP::agent $VERSION;

# Preloaded methods go here.

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__

=head1 NAME

NetSNMP::agent - Perl extension for the net-snmp agent.

=head1 SYNOPSIS

  use NetSNMP::agent;

  my $agent = new NetSNMP::agent('Name' => 'my_agent_name');


=head1 DESCRIPTION

This module implements an API set to make a SNMP agent act as a snmp
agent, a snmp subagent (using the AgentX subagent protocol) and/or
embedded perl-APIs directly within the traditional net-snmp agent demon.

Also see the tutorial about the genaral Net-SNMP C API, which this
module implements in a perl-way, and a perl specific tutorial at:

  http://www.net-snmp.org/tutorial-5/toolkit/

=head1 EXAMPLES

=head2 Sub-agent example

    	use NetSNMP::agent (':all');
    	use NetSNMP::ASN qw(ASN_OCTET_STR);

        my $value = "hello world";
	sub myhandler {
	    my ($handler, $registration_info, $request_info, $requests) = @_;
	    my $request;

	    for($request = $requests; $request; $request = $request->next()) {
		my $oid = $request->getOID();
		if ($request_info->getMode() == MODE_GET) {
		    # ... generally, you would calculate value from oid
		    if ($oid == new NetSNMP::OID(".1.3.6.1.4.1.8072.9999.9999.7375.1.0")) {
			$request->setValue(ASN_OCTET_STR, $value);
		    }
		} elsif ($request_info->getMode() == MODE_GETNEXT) {
		    # ... generally, you would calculate value from oid
		    if ($oid < new NetSNMP::OID(".1.3.6.1.4.1.8072.9999.9999.7375.1.0")) {
			$request->setOID(".1.3.6.1.4.1.8072.9999.9999.7375.1.0");
			$request->setValue(ASN_OCTET_STR, $value);
		    }
		} elsif ($request_info->getMode() == MODE_SET_RESERVE1) {
		    if ($oid != new NetSNMP::OID(".1.3.6.1.4.1.8072.9999.9999.7375.1.0")) {  # do error checking here
			$request->setError($request_info, SNMP_ERR_NOSUCHNAME);
		    }
		} elsif ($request_info->getMode() == MODE_SET_ACTION) {
		    # ... (or use the value)
		    $value = $request->getValue();
		}
	    }

	}

	my $agent = new NetSNMP::agent(
				# makes the agent read a my_agent_name.conf file
    				'Name' => "my_agent_name",
    				'AgentX' => 1
    				);
    	$agent->register("my_agent_name", ".1.3.6.1.4.1.8072.9999.9999.7375",
                         \&myhandler);

	my $running = 1;
	while($running) {
    		$agent->agent_check_and_process(1);
	}

	$agent->shutdown();


=head2 Embedded agent example

        # place this in a .pl file, and then in your snmpd.conf file put:
        #    perl do '/path/to/file.pl';

	use NetSNMP::agent;
	my $agent;

	sub myhandler {
	    my ($handler, $registration_info, $request_info, $requests) = @_;
	    # ...
	}

	$agent = new NetSNMP::agent(
    				'Name' => 'my_agent_name'
    				);

    	$agent->register("my_agent_name", ".1.3.6.1.4.1.8072.9999.9999.7375",
                         \&myhandler);

	$agent->main_loop();


=head1 CONSTRUCTOR

    new ( OPTIONS )
	This is the constructor for a new NetSNMP::agent object.

    Possible options are:

    	Name	- Name of the agent (optional, defaults to "perl")
                  (The snmp library will read a NAME.conf snmp
                  configuration file based on this argument.)
    	AgentX	- Make us a sub-agent (0 = false, 1 = true)
                  (The Net-SNMP master agent must be running first)
    	Ports	- Ports this agent will listen on (EG: "udp:161,tcp:161")

    Example:

	$agent = new NetSNMP::agent(
    				 'Name' => 'my_agent_name',
    				 'AgentX' => 1
    				 );


=head1 METHODS

    register (NAME, OID, \&handler_routine )
    	Registers the callback handler with given OID.

    	$agent->register();

	A return code of 0 indicates no error.

    agent_check_and_process ( BLOCKING )
    	Run one iteration of the main loop.

    	BLOCKING - Blocking or non-blocking call. 1 = true, 0 = false.

    	$agent->agent_check_and_process(1);

    main_loop ()
    	Runs the agent in a loop. Does not return.

    shutdown ()
	Nicely shuts down the agent or sub-agent.

	$agent->shutdown();

=head1 HANDLER CALLBACKS

    handler ( HANDLER, REGISTRATION_INFO, REQUEST_INFO, REQUESTS )

    	The handler is called with the following parameters:

	HANDLER 		- FIXME
    	REGISTRATION_INFO 	- what are the correct meanings of these?
    	REQUEST_INFO		-
    	REQUESTS		-

    Example handler:

	sub myhandler {
	    my ($handler, $reg_info, $request_info, $requests) = @_;
	    # ...
	}

The handler subroutine will be called when a SNMP request received by
the agent for anything below the registered OID.  The handler is
passed 4 arguments: $handler, $registration_info, $request_info,
$requests.  These match the arguments passed to the C version of the
same API.  Note that they are not entirely complete objects but are
functional "enough" at this point in time.

=head2 $request_info object functions

    getMode ()
    	Returns the mode of the request. See the MODES section for
    	list of valid modes.

	$mode = $request->getMode();

=head2 $registration_info object functions

    getRootOID ()
	Returns a NetSNMP::OID object that describes the registration
	point that the handler is getting called for (in case you
	register one handler function with multiple OIDs, which should
	be rare anyway)

    	$root_oid = $request->getRootOID();

=head2 $request object functions

    next ()
    	Returns the next request in the list or undef if there is no
    	next request.

    	$request = $request->next();

    getOID ()
	Returns the oid of the request (a NetSNMP::OID class).

	$oid = $request->getOID();

    setOID (new NetSNMP::OID("someoid"))
	Sets the OID of the request to a passed oid value.  This
	should generally only be done during handling of GETNEXT
	requests.

	$request->setOID(new NetSNMP::OID("someoid"));

    getValue ()
	Returns the value of the request. Used for example when
	setting values.

    	$value = $request->getValue();

    	FIXME: how to get the type of the value? Is it even available?
               [Wes: no, not yet.]

    setValue ( TYPE, DATA )
	Sets the data to be returned to the daemon.

    	Returns 1 on success, 0 on error.

    	TYPE - Type of the data. See NetSNMP::ASN for valid types.
    	DATA - The data to return.

	$ret = $request->setValue(ASN_OCTET_STR, "test");

    setError ( REQUEST_INFO, ERROR_CODE )
	Sets the given error code for the request. See the ERROR CODES
	section for list of valid codes.

    	$request->setError($request_info, SNMP_ERR_NOTWRITABLE);

    getProcessed ()
    	The processed flag indicates that a request does not need to
    	be dealt with because someone else (a higher handler) has
    	dealt with it already.

    	$processed = $request->getProcessed();

    setProcessed ( PROCESSED )
	Sets the processed flag flag in the request.  You generally
	should not have to set this yourself.

	PROCESSED - 0 = false, 1 = true

	$request->setProcessed(1);

    getDelegated ()
	If you can handle a request in the background or at a future
	time (EG, you're waiting on a file handle, or network traffic,
	or ...), the delegated flag can be set in the request.  When
	the request is processed in the future the flag should be set
	back to 0 so the agent will know that it can wrap up the
	original request and send it back to the manager.  This has
	not been tested within perl, but it hopefully should work.

	$delegated = $request->getDelegated();

    setDelegated ( DELEGATED )
    	Sets the delegated flag.

    	DELEGATED - 0 = false, 1 = true

    	$request->setDelegated(1);

    getRepeat ()
	The repeat flag indicates that a getbulk operation is being
	handled and this indicates how many answers need to be
	returned.  Generally, if you didn't register to directly
	handle getbulk support yourself, you won't need to deal with
	this value.

    	$repeat = $request->getRepeat();

    setRepeat ( REPEAT )
	Sets the repeat count (decrement after answering requests if
	you handle getbulk requests yourself)

	REPEAT -  repeat count FIXME

	$request->setRepeat(5);

    getSourceIp ()

	Gets the IPv4 address of the device making the request to the handler.

	use Socket;
	print "Source: ", inet_ntoa($request->getSourceIp()), "\n";

    getDestIp ()

	Gets the IPv4 address of the destination that the request was sent to.

	use Socket;
	print "Destination: ", inet_ntoa($request->getDestIp()), "\n";

=head1 MODES

	MODE_GET
	MODE_GETBULK
	MODE_GETNEXT
	MODE_SET_ACTION
	MODE_SET_BEGIN
	MODE_SET_COMMIT
	MODE_SET_FREE
	MODE_SET_RESERVE1
	MODE_SET_RESERVE2
	MODE_SET_UNDO

=head1 ERROR CODES

	SNMP_ERR_NOERROR
	SNMP_ERR_TOOBIG
	SNMP_ERR_NOSUCHNAME
	SNMP_ERR_BADVALUE
	SNMP_ERR_READONLY
	SNMP_ERR_GENERR
	SNMP_ERR_NOACCESS
	SNMP_ERR_WRONGTYPE
	SNMP_ERR_WRONGLENGTH
	SNMP_ERR_WRONGENCODING
	SNMP_ERR_WRONGVALUE
	SNMP_ERR_NOCREATION
	SNMP_ERR_INCONSISTENTVALUE
	SNMP_ERR_RESOURCEUNAVAILABLE
	SNMP_ERR_COMMITFAILED
	SNMP_ERR_UNDOFAILED
	SNMP_ERR_AUTHORIZATIONERROR
	SNMP_ERR_NOTWRITABLE

=head1 AUTHOR

Please mail the net-snmp-users@lists.sourceforge.net mailing list for
help, questions or comments about this module.

Module written by:
   Wes Hardaker  <hardaker@users.sourceforge.net>

Documentation written by:
   Toni Willberg <toniw@iki.fi>
   Wes Hardaker  <hardaker@users.sourceforge.net>

=head1 SEE ALSO

NetSNMP::OID(3), NetSNMP::ASN(3), perl(1).

=cut
