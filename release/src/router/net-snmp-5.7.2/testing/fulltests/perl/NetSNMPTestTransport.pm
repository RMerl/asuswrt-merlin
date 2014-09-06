BEGIN {
    if (exists($ENV{'srcdir'})) {
	push @INC, "$ENV{'srcdir'}/testing/fulltests/support";
    } elsif (-d "fulltests/support") {
	push @INC, "fulltests/support";
    } elsif (-d "../support") {
	push @INC, "../support";
    }
}

package NetSNMPTestTransport;

use NetSNMPTest;
use Test;
use SNMP;

our @ISA = qw(NetSNMPTest);

sub run_tests {
    my ($self) = @_;

    plan(tests => 2);

    # set it up with a snmpv3 USM user
    $self->config_agent("createuser testuser MD5 notareallpassword");
    $self->config_agent("rwuser testuser");
    $self->config_agent("syscontact itworked");

    $self->DIE("failed to start the agent") if (!$self->start_agent());

    # now create a session to test things with
    my $session = new SNMP::Session(DestHost => $self->{'agentaddress'},
				    Version => '3',
				    SecName => 'testuser',
				    SecLevel => 'authNoPriv',
				    AuthProto => 'MD5',
				    AuthPass => 'notareallpassword');

    ok(ref($session), 'SNMP::Session', "created a session");

    ######################################################################
    # GET test
    if (ref($session) eq 'SNMP::Session') {
	$value = $session->get('sysContact.0');
	ok($value, 'itworked');
    }

    ######################################################################
    # cleanup
    $self->stop_agent();
}


1;
