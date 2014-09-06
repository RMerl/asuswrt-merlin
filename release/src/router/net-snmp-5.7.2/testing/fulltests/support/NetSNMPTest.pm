package NetSNMPTest;

use File::Temp qw(tempfile tempdir);
use IO::File;
use Data::Dumper;
use strict;

sub new {
    my $type = shift;
    my ($class) = ref($type) || $type;
    my $self = {};
    %$self = @_;
    bless($self, $class);
    $self->init();
    return $self;
}

sub init {
    my ($self) = @_;
    $self->{'dir'} = tempdir();
    print "# using tempdir $self->{dir}\n";

    foreach my $suffix (qw(conf pid log out)) {
	$self->{"snmpd.$suffix"} ||= $self->{'dir'} . "/snmpd.$suffix";
    }
    $self->{'snmp.conf'} ||= $self->{'dir'} . "/snmp.conf";

    $self->{'confdir'} ||= $self->{'dir'};
    $self->{'persistentdir'} ||= $self->{'dir'} . "/persistent";
}

sub config_file {
    my ($self, $file, $string) = @_;
    my $fh = new IO::File (">> $file");
    $fh->print($string);
    if ($string !~ /\n$/) {
	$fh->print("\n");
    }
    $fh->close();
}

sub config_agent {
    my ($self, $string) = @_;
    $self->config_file($self->{'snmpd.conf'}, $string);
}

sub config_app {
    my ($self, $string) = @_;
    $self->config_file($self->{'snmp.conf'}, $string);
}

sub require_feature {
    my ($self, $feature) = @_;
    my $srcdir = $ENV{'srcdir'} || "..";
    my $fh = new IO::File("$srcdir/include/net-snmp/net-snmp-config.h");
    while (<$fh>) {
	if (/#define $feature 1/) {
	    $fh->close();
	    return 1;
	}
    }
    print "1..0 # SKIP missing $feature\n";
    exit;
}

sub start_agent {
    my ($self, $flags) = @_;

    $flags ||= $self->{'snmpdflags'};

    $ENV{'SNMPCONFPATH'} = $self->{'confdir'};
    $ENV{'SNMP_PERSISTENT_DIR'} = $self->{'peristentdir'};

    my $cmd = "snmpd $flags -r -U -p $self->{'snmpd.pid'} -Lf $self->{'snmpd.log'} $self->{'agentaddress'} > $self->{'snmpd.out'} 2>&1";
    System("$cmd &");

    sleep(1);

    return $self->wait_for($self->{'snmpd.log'}, "NET-SNMP version");
}

sub stop_agent {
    my ($self) = @_;
    my $pidfile = new IO::File "$self->{'snmpd.pid'}";
    my $pid = <$pidfile>;
    kill("TERM", $pid);
    $self->wait_for($self->{'snmpd.log'}, 'shutting down');
}

# returns 1 on success, 0 on failure
sub wait_for {
    my ($self, $filename, $regexp, $maxtime) = @_;
    my $fh = new IO::File "$filename";
    return 0 if (!$fh);

    $maxtime = 10 if (!defined($maxtime));

    my $timecount = 0;
    # print "# reading from: $filename\n";
    while (1) {
	my $line = <$fh>;
	# print "# line: $line\n";
	if ($line eq "") {
	    if ($timecount == $maxtime) {
		return 0;
	    }
	    sleep(1);
	    print "# sleeping ...\n";
	    $fh->clearerr();
	    $timecount++;
	} else {
	    chomp($line);
	    if ($line =~ /$regexp/) {
		return 1;
	    }
	}
    }
    return 0;
}

sub Debug {
    print  "# " . join(" ",@_);
}

sub System {
    my ($cmd) = @_;
    Debug("running: ", $cmd, "\n");
    system($cmd);
}

sub DIE {
    my $self = shift;
    $self->stop_agent();
    die @_;
}

1;

=pod

=head1 NAME

NetSNMPTest - simple testing module for testing perl tests

=head1 USAGE

use NetSNMPTest;
use Test;
use SNMP;

my $destination = "udp:localhost:9876";

my $test = new NetSNMPTest(agentaddress => $destination);

$test->require_feature("SOME_IFDEF_FROM_NET_SNMP_CONFIG_H");

$test->config_agent("rocommunity public");
$test->config_agent("syscontact testvalue");
$test->DIE("failed to start the agent") if (!$test->start_agent());

my $session = new SNMP::Session(DestHost => $destination,
                                Version => '2c',
                                Community => 'public');

my $value = $session->get('sysContact.0');
plan(tests => 1);
ok($value, 'testvalue');

$test->stop_agent();

=cut

