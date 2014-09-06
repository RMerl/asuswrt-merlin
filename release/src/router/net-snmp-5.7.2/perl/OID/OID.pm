package NetSNMP::OID;

use strict;
use warnings;
use Carp;

require Exporter;
require DynaLoader;
use AutoLoader;

sub compare($$);

use overload
    '<=>' => \&compare,
    'cmp' => \&oidstrcmp,
    '""' => \&quote_oid,
    '+' => \&add,
;

use SNMP;

sub quote_oid {
    return $_[0]->{'oidptr'}->to_string();
}

sub length {
    return $_[0]->{'oidptr'}->length();
}

sub get_indexes {
    return $_[0]->{'oidptr'}->get_indexes();
}

sub append {
    my $this = shift;
    my $str = shift;

    if (ref($str) eq 'NetSNMP::OID') {
	return $this->{'oidptr'}->append_oid($str->{'oidptr'});
    }
    $str = "." . $str if ($str =~ /^\d+/);
    if ($str =~ /^[.\d]+/) {
	# oid segment
	return $this->{'oidptr'}->append($str);
    }
    if ($str =~ /^\"(.*)\"$/) {
	# string index
	my $newstr = "." . CORE::length($1);
	map { $newstr .= ".$_" } unpack("c*",$1);
	return $this->{'oidptr'}->append($newstr);
    }
    if ($str =~ /^\'(.*)\'$/) {
	# string index, implied
	my $newstr;
	map { $newstr .= ".$_" } unpack("c*",$1);
	return $this->{'oidptr'}->append($newstr);
    }
    # Just Parse it...
    return $this->{'oidptr'}->append($str);
}

sub add {
    my $this = shift;
    my $str = shift;
    my ($newoid, %newhash);
    $newoid = \%newhash;
    $newoid->{'oidptr'} = $this->{'oidptr'}->clone();
    bless($newoid, ref($this));
    $newoid->append($str);
    return $newoid;
}

use vars qw(@ISA %EXPORT_TAGS @EXPORT_OK @EXPORT $VERSION $AUTOLOAD);

@ISA = qw(Exporter DynaLoader);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use NetSNMP::OID ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
%EXPORT_TAGS = ( 'all' => [ qw(
	snmp_oid_compare
        compare
) ] );

@EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

@EXPORT = qw(
	snmp_oid_compare
        compare
);
$VERSION = '5.0702';

sub new {
    my $type = shift;
    my $arg = shift;
    if (!$arg) {
	$arg = $type;
	$type = "NetSNMP::OID";
    }
    SNMP::init_snmp("perl");
    my $ptr = NetSNMP::OID::newptr($arg);
    if ($ptr) {
      return newwithptr($type, $ptr);
    }
}

sub newwithptr {
    my $type = shift;
    my $ptr = shift;
    my $self = {};
    if (!$ptr) {
	$ptr = $type;
	$type = "NetSNMP::OID";
    }
    SNMP::init_snmp("perl");
    $self->{'oidptr'} = $ptr;
    bless($self, $type);
    return $self;
}

sub snmp_oid_compare($$) {
    my ($oid1, $oid2) = @_;
    return _snmp_oid_compare($oid1->{oidptr}, $oid2->{oidptr});
}

sub compare($$) {
    my ($v1, $v2) = @_;
    snmp_oid_compare($v1, $v2);
}

sub oidstrcmp {
    my ($v1, $v2) = @_;
    $v1->{'oidptr'}->to_string cmp $v2->{'oidptr'}->to_string;
}

sub to_array($) {
    my $self = shift;
    return $self->{oidptr}->to_array();
}

sub DESTROY {}

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
	    croak "Your vendor has not defined NetSNMP::OID macro $constname";
	}
    }
    {
	no strict 'refs';
	# Fixed between 5.005_53 and 5.005_61
	if ($] >= 5.00561) {
	    *$AUTOLOAD = sub () { $val };
	}
	else {
	    *$AUTOLOAD = sub { $val };
	}
    }
    goto &$AUTOLOAD;
}

bootstrap NetSNMP::OID $VERSION;

# Preloaded methods go here.

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__

=head1 NAME

NetSNMP::OID - Perl extension for manipulating OIDs

=head1 SYNOPSIS

  use NetSNMP::OID;

  my $oid = new NetSNMP::OID('sysContact.0');

  if ($oid < new NetSNMP::OID('ifTable')) {
      do_something();
  }

  my @numarray = $oid->to_array();

  # appending oids
  $oid = new NetSNMP::OID('.1.3');
  $oid += ".6.1";
  # -> .1.3.6.1

  # appending index strings

  $oid2 = $oid + "\"wes\"";
  # -> .1.3.6.1.3.119.101.115

  $oid3 = $oid + "\'wes\'";
  # -> .1.3.6.1.119.101.115

  $len = $oid3->length();
  # -> 7

  # retrieving indexes from an oid:
  $arrayref = $tableoid->get_indexes()

=head1 DESCRIPTION

The NetSNMP::OID module is a simple wrapper around a C-based net-snmp
oid (which is an array of unsigned integers).  The OID is internally
stored as a C array of integers for speed purposes when doing
comparisons, etc.

The standard logical expression operators (<, >, ==, ...) are
overloaded such that lexographical comparisons may be done with them.

The + operator is overloaded to allow you to append stuff on to the
end of a OID, like index segments of a table, for example.

=head2 EXPORT

int snmp_oid_compare(oid1, oid2)
int compare(oid1, oid2)

=head1 AUTHOR

Wes Hardaker, E<lt>hardaker@users.sourceforge.netE<gt>

=head1 SEE ALSO

L<SNMP>, L<perl>.

=head1 Copyright

Copyright (c) 2002 Networks Associates Technology, Inc.  All
Rights Reserved.  This program is free software; you can
redistribute it and/or modify it under the same terms as Perl
itself.

=cut
