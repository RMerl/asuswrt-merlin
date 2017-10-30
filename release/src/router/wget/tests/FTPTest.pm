package FTPTest;

use strict;
use warnings;

use FTPServer;
use WgetTests;

our @ISA = qw(WgetTest);
my $VERSION = 0.01;

{
    my %_attr_data = (    # DEFAULT
    );

    sub _default_for
    {
        my ($self, $attr) = @_;
        return $_attr_data{$attr} if exists $_attr_data{$attr};
        return $self->SUPER::_default_for($attr);
    }

    sub _standard_keys
    {
        my ($self) = @_;
        ($self->SUPER::_standard_keys(), keys %_attr_data);
    }
}

sub _setup_server
{
    my $self = shift;

    $self->{_server} = FTPServer->new(
                             input           => $self->{_input},
                             server_behavior => $self->{_server_behavior},
                             LocalAddr       => 'localhost',
                             ReuseAddr       => 1,
                             rootDir => "$self->{_workdir}/$self->{_name}/input"
      )
      or die "Cannot create server!!!";
}

sub _launch_server
{
    my $self       = shift;
    my $synch_func = shift;

    $self->{_server}->run($synch_func);
}

sub _substitute_port
{
    my $self = shift;
    my $ret  = shift;
    $ret =~ s/\Q{{port}}/$self->{_server}->sockport/eg;
    return $ret;
}

1;

# vim: et ts=4 sw=4
