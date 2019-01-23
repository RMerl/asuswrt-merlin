package HTTPTest;

use strict;
use warnings;

use HTTPServer;
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
    $self->{_server} = HTTPServer->new(LocalAddr => 'localhost',
                                       ReuseAddr => 1)
      or die "Cannot create server!!!";
}

sub _launch_server
{
    my $self       = shift;
    my $synch_func = shift;

    $self->{_server}->run($self->{_input}, $synch_func);
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
