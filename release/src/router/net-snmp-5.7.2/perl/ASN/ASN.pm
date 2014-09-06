package NetSNMP::ASN;

use strict;
use warnings;
use Carp;

require Exporter;
require DynaLoader;
use AutoLoader;

use vars qw(@ISA %EXPORT_TAGS @EXPORT_OK @EXPORT $VERSION $AUTOLOAD);

@ISA = qw(Exporter DynaLoader);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use NetSNMP::ASN ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
%EXPORT_TAGS = ( 'all' => [ qw(
	ASN_APPLICATION
	ASN_BIT_STR
	ASN_BOOLEAN
	ASN_COUNTER
	ASN_COUNTER64
	ASN_DOUBLE
	ASN_FLOAT
	ASN_GAUGE
	ASN_INTEGER
	ASN_INTEGER64
	ASN_IPADDRESS
	ASN_NULL
	ASN_OBJECT_ID
	ASN_OCTET_STR
	ASN_OPAQUE
	ASN_SEQUENCE
	ASN_SET
	ASN_TIMETICKS
	ASN_UNSIGNED
	ASN_UNSIGNED64
) ] );

@EXPORT_OK = ( @{ $EXPORT_TAGS{'all'} } );

@EXPORT = qw(
	ASN_APPLICATION
	ASN_BIT_STR
	ASN_BOOLEAN
	ASN_COUNTER
	ASN_COUNTER64
	ASN_DOUBLE
	ASN_FLOAT
	ASN_GAUGE
	ASN_INTEGER
	ASN_INTEGER64
	ASN_IPADDRESS
	ASN_NULL
	ASN_OBJECT_ID
	ASN_OCTET_STR
	ASN_OPAQUE
	ASN_SEQUENCE
	ASN_SET
	ASN_TIMETICKS
	ASN_UNSIGNED
	ASN_UNSIGNED64
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
	    croak "Your vendor has not defined NetSNMP::ASN macro $constname";
	}
    }
    {
	no strict 'refs';
	# Fixed between 5.005_53 and 5.005_61
# 	if ($] >= 5.00561) {
# 	    *$AUTOLOAD = sub () { $val };
# 	}
# 	else {
	    *$AUTOLOAD = sub { $val };
# 	}
    }
    goto &$AUTOLOAD;
}

bootstrap NetSNMP::ASN $VERSION;

# Preloaded methods go here.

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__

=head1 NAME

NetSNMP::ASN - Perl extension for SNMP ASN.1 types

=head1 SYNOPSIS

  use NetSNMP::ASN qw(:all);
  my $asn_int = ASN_INTEGER;

=head1 DESCRIPTION

The NetSNMP::ASN module provides the ASN.1 types for SNMP.

=head2 EXPORT

None by default.

=head2 Exportable constants

  ASN_APPLICATION
  ASN_BIT_STR
  ASN_BOOLEAN
  ASN_COUNTER
  ASN_COUNTER64
  ASN_DOUBLE
  ASN_FLOAT
  ASN_GAUGE
  ASN_INTEGER
  ASN_INTEGER64
  ASN_IPADDRESS
  ASN_NULL
  ASN_OBJECT_ID
  ASN_OCTET_STR
  ASN_OPAQUE
  ASN_SEQUENCE
  ASN_SET
  ASN_TIMETICKS
  ASN_UNSIGNED
  ASN_UNSIGNED64


=head1 AUTHOR

Wes Hardaker, E<lt>hardaker@users.sourceforge.netE<gt>

=head1 SEE ALSO

SNMP(3pm), NetSNMP::OID(3pm)

perl(1).

=cut
