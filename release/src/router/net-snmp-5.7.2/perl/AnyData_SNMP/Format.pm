package AnyData::Format::SNMP;
#
# AnyData interface to SNMP queries
#

use strict;
use warnings;
use AnyData::Format::Base;
use vars qw( @ISA );
@AnyData::Format::SNMP::ISA = qw( AnyData::Format::Base );
use Data::Dumper;

sub storage_type {
#    print "calling storage type\n"; 'SNMP';
}

sub new {
#    print "new format: ", Dumper(@_), "\n";
    my $class = shift;
    my $self  = shift ||  {};
    bless $self, $class;
    $self->{'storage'} = 'SNMP';
    $self->{'has_update_function'} = 'SNMP';
#    print Dumper($self), "\n";
    return $self;
    2;
}

sub get_col_names {
#    print "get_col_names\n";
    # XXX: get mib column names
    2;
}

sub seek_first_record {
#    print "seek_first\n";
    my $self = shift;
    my $var = [$self->{'mibnode'}];
    $self->{'session'}->getnext($var);
    2;
}

sub get_pos {
#    print "get_pos\n";
    2;
}

sub go_pos {
#    print "go_pos\n";
    2;
}

sub delete_record {
#    print "del_rec\n";
    2;
}

sub get_record {
#    print "get_record\n";
    2;
}

sub push_row {
#    print "push_row\n";
    2;
}

sub truncate {
#    print "truncate\n";
    2;
}

sub close_table {
#    print "close_table\n";
    2;
}

sub drop {
#    print "drop\n";
    2;
}

sub seek {
#    print "seek\n";
    2;
}

sub write_fields {
#    print STDERR "write_fields: ",Dumper(\@_), "\n";
    my $self   = shift;
    my @ary = @_;
    return \@ary;
}
sub read_fields {
#    print STDERR "read_fields: ",Dumper(\@_), "\n";
    my $self   = shift;
    my $aryref = shift;
    return @$aryref;
}

sub get_data {
#    print "get_data\n";
    2;
}

sub init_parser {
#    print "init_parser\n";
    2;
}
