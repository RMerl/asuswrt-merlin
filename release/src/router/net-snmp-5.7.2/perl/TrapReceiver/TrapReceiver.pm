package NetSNMP::TrapReceiver;

use 5.00006;
use strict;
use Carp;

require Exporter;
require DynaLoader;

use AutoLoader;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK %EXPORT_TAGS $AUTOLOAD);
@ISA = qw(Exporter
	DynaLoader);

require NetSNMP::OID;


# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use NetSNMP::TrapReceiver ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
%EXPORT_TAGS = ( 'all' => [ qw(
	NETSNMPTRAPD_AUTH_HANDLER
	NETSNMPTRAPD_HANDLER_BREAK
	NETSNMPTRAPD_HANDLER_FAIL
	NETSNMPTRAPD_HANDLER_FINISH
	NETSNMPTRAPD_HANDLER_OK
	NETSNMPTRAPD_POST_HANDLER
	NETSNMPTRAPD_PRE_HANDLER
	netsnmp_add_default_traphandler
	netsnmp_add_global_traphandler
	netsnmp_add_traphandler
) ] );

@EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

@EXPORT = qw(
	NETSNMPTRAPD_AUTH_HANDLER
	NETSNMPTRAPD_HANDLER_BREAK
	NETSNMPTRAPD_HANDLER_FAIL
	NETSNMPTRAPD_HANDLER_FINISH
	NETSNMPTRAPD_HANDLER_OK
	NETSNMPTRAPD_POST_HANDLER
	NETSNMPTRAPD_PRE_HANDLER
);

$VERSION = '5.0702';

# sub new {
#     my $type = shift;
#     my ($self);
#     %$self = @_;
#     bless($self, $type);
#     return $self;
# }

# sub register($$$$) {
#     my ($self, $oid, $sub) = @_;
#     my $reg = NetSNMP::TrapReceiver::registration::new($oid, $sub);
#     if ($reg) {
# 	$reg->register();
# 	$self->{'regobjs'}{$name} = $reg;
#     }
#     return $reg;
# }

sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.

    my $constname;
    ($constname = $AUTOLOAD) =~ s/.*:://;
    croak "&NetSNMP::TrapReceiver::constant not defined" if $constname eq 'constant';
    my ($error, $val) = constant($constname);
    if ($error) { croak $error; }
    {
	no strict 'refs';
	# Fixed between 5.005_53 and 5.005_61
#XXX	if ($] >= 5.00561) {
#XXX	    *$AUTOLOAD = sub () { $val };
#XXX	}
#XXX	else {
	    *$AUTOLOAD = sub { $val };
#XXX	}
    }
    goto &$AUTOLOAD;
}

bootstrap NetSNMP::TrapReceiver $VERSION;

# Preloaded methods go here.

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__

=head1 NAME

NetSNMP::TrapReceiver - Embedded perl trap handling for Net-SNMP's snmptrapd

=head1 SYNOPSIS

Put the following lines in your snmptrapd.conf file:

  perl NetSNMP::TrapReceiver::register("trapOID", \&myfunc);

=head1 ABSTRACT

The NetSNMP::TrapReceiver module is used to register perl
subroutines into the Net-SNMP snmptrapd process.  Net-SNMP MUST have
been configured using --enable-embedded-perl.  Registration of
functions is then done through the snmptrapd.conf configuration
file.  This module can NOT be used in a normal perl script to
receive traps.  It is intended solely for embedded use within the
snmptrapd demon.

=head1 DESCRIPTION

Within the snmptrapd.conf file, the keyword "perl" may be used to call
any perl expression and using this ability, you can use the
NetSNMP::TrapReceiver module to register functions which will be
called every time a given notification (a trap or an inform) is
received.  Registered functions are called with 2 arguments.  The
first is a reference to a hash containing information about how the
trap was received (what version of the SNMP protocol was used, where
it came from, what SNMP user name or community name it was sent under,
etc).  The second argument is a reference to an array containing the
variable bindings (OID and value information) that define the
noification itself.  Each variable is itself a reference to an array
containing three values: a NetSNMP::OID object, the value that came
associated with it, and the value's numeric type (see NetSNMP::ASN for
further details on SNMP typing information).

Registered functions should return one of the following values:

=over 2

=item NETSNMPTRAPD_HANDLER_OK

Handling the trap succeeded, but lets the snmptrapd demon check for
further appropriate handlers.

=item NETSNMPTRAPD_HANDLER_FAIL

Handling the trap failed, but lets the snmptrapd demon check for
further appropriate handlers.

=item NETSNMPTRAPD_HANDLER_BREAK

Stops evaluating the list of handlers for this specific trap, but lets
the snmptrapd demon apply global handlers.

=item NETSNMPTRAPD_HANDLER_FINISH

Stops searching for further appropriate handlers.

=back

If a handler function does not return anything appropriate or even
nothing at all, a return value of NETSNMPTRAPD_HANDLER_OK is assumed.

Subroutines are registered using the NetSNMP::TrapReceiver::register
function, which takes two arguments.  The first is a string describing
the notification you want to register for (such as "linkUp" or
"MyMIB::MyTrap" or ".1.3.6.1.4.1.2021....").  Two special keywords can
be used in place of an OID: "default" and "all".  The "default"
keyword indicates you want your handler to be called in the case where
no other handlers are called.  The "all" keyword indicates that the
handler should ALWAYS be called for every notification.


=head1 EXAMPLE

As an example, put the following code into a file (say
"/usr/local/share/snmp/mytrapd.pl"):

  #!/usr/bin/perl

  sub my_receiver {
      print "********** PERL RECEIVED A NOTIFICATION:\n";

      # print the PDU info (a hash reference)
      print "PDU INFO:\n";
      foreach my $k(keys(%{$_[0]})) {
        if ($k eq "securityEngineID" || $k eq "contextEngineID") {
          printf "  %-30s 0x%s\n", $k, unpack('h*', $_[0]{$k});
        }
        else {
          printf "  %-30s %s\n", $k, $_[0]{$k};
        }
      }

      # print the variable bindings:
      print "VARBINDS:\n";
      foreach my $x (@{$_[1]}) { 
	  printf "  %-30s type=%-2d value=%s\n", $x->[0], $x->[2], $x->[1]; 
      }
  }

  NetSNMP::TrapReceiver::register("all", \&my_receiver) || 
    warn "failed to register our perl trap handler\n";

  print STDERR "Loaded the example perl snmptrapd handler\n";

Then, put the following line in your snmprapd.conf file:

  perl do "/usr/local/share/snmp/mytrapd.pl";

Start snmptrapd (as root, and the following other opions make it stay
in the foreground and log to stderr):

  snmptrapd -f -Le

You should see it start up and display the final message from the end
of the above perl script:

  Loaded the perl snmptrapd handler
  2004-02-11 10:08:45 NET-SNMP version 5.2 Started.

Then, if you send yourself a fake trap using the following example command:

  snmptrap -v 2c -c mycommunity localhost 0 linkUp ifIndex.1 i 1 \
      ifAdminStatus.1 i up ifOperStatus.1 i up ifDescr s eth0

You should see the following output appear from snmptrapd as your perl
code gets executed:

  ********** PERL RECEIVED A NOTIFICATION:
  PDU INFO:
    notificationtype               TRAP
    receivedfrom                   127.0.0.1
    version                        1
    errorstatus                    0
    messageid                      0
    community                      mycommunity
    transactionid                  2
    errorindex                     0
    requestid                      765160220
  VARBINDS:
    sysUpTimeInstance              type=67 value=0:0:00:00.00
    snmpTrapOID.0                  type=6  value=linkUp
    ifIndex.1                      type=2  value=1
    ifAdminStatus.1                type=2  value=1
    ifOperStatus.1                 type=2  value=1
    ifDescr                        type=4  value="eth0"

=head1 EXPORT

None by default.

# =head2 Exportable constants

#   NETSNMPTRAPD_AUTH_HANDLER
#   NETSNMPTRAPD_HANDLER_BREAK
#   NETSNMPTRAPD_HANDLER_FAIL
#   NETSNMPTRAPD_HANDLER_FINISH
#   NETSNMPTRAPD_HANDLER_OK
#   NETSNMPTRAPD_POST_HANDLER
#   NETSNMPTRAPD_PRE_HANDLER

=head1 SEE ALSO

NetSNMP::OID, NetSNMP::ASN

snmptrapd.conf(5) for configuring the Net-SNMP trap receiver.

snmpd.conf(5) for configuring the Net-SNMP snmp agent for sending traps.

http://www.Net-SNMP.org/

=head1 AUTHOR

W. Hardaker, E<lt>hardaker@users.sourceforge.netE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright 2004 by W. Hardaker

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
