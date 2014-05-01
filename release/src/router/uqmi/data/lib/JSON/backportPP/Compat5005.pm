package # This is JSON::backportPP
    JSON::backportPP5005;

use 5.005;
use strict;

my @properties;

$JSON::PP5005::VERSION = '1.10';

BEGIN {

    sub utf8::is_utf8 {
        0; # It is considered that UTF8 flag off for Perl 5.005.
    }

    sub utf8::upgrade {
    }

    sub utf8::downgrade {
        1; # must always return true.
    }

    sub utf8::encode  {
    }

    sub utf8::decode {
    }

    *JSON::PP::JSON_PP_encode_ascii      = \&_encode_ascii;
    *JSON::PP::JSON_PP_encode_latin1     = \&_encode_latin1;
    *JSON::PP::JSON_PP_decode_surrogates = \&_decode_surrogates;
    *JSON::PP::JSON_PP_decode_unicode    = \&_decode_unicode;

    # missing in B module.
    sub B::SVp_IOK () { 0x01000000; }
    sub B::SVp_NOK () { 0x02000000; }
    sub B::SVp_POK () { 0x04000000; }

    $INC{'bytes.pm'} = 1; # dummy
}



sub _encode_ascii {
    join('', map { $_ <= 127 ? chr($_) : sprintf('\u%04x', $_) } unpack('C*', $_[0]) );
}


sub _encode_latin1 {
    join('', map { chr($_) } unpack('C*', $_[0]) );
}


sub _decode_surrogates { # from http://homepage1.nifty.com/nomenclator/unicode/ucs_utf.htm
    my $uni = 0x10000 + (hex($_[0]) - 0xD800) * 0x400 + (hex($_[1]) - 0xDC00); # from perlunicode
    my $bit = unpack('B32', pack('N', $uni));

    if ( $bit =~ /^00000000000(...)(......)(......)(......)$/ ) {
        my ($w, $x, $y, $z) = ($1, $2, $3, $4);
        return pack('B*', sprintf('11110%s10%s10%s10%s', $w, $x, $y, $z));
    }
    else {
        Carp::croak("Invalid surrogate pair");
    }
}


sub _decode_unicode {
    my ($u) = @_;
    my ($utf8bit);

    if ( $u =~ /^00([89a-f][0-9a-f])$/i ) { # 0x80-0xff
         return pack( 'H2', $1 );
    }

    my $bit = unpack("B*", pack("H*", $u));

    if ( $bit =~ /^00000(.....)(......)$/ ) {
        $utf8bit = sprintf('110%s10%s', $1, $2);
    }
    elsif ( $bit =~ /^(....)(......)(......)$/ ) {
        $utf8bit = sprintf('1110%s10%s10%s', $1, $2, $3);
    }
    else {
        Carp::croak("Invalid escaped unicode");
    }

    return pack('B*', $utf8bit);
}


sub JSON::PP::incr_text {
    $_[0]->{_incr_parser} ||= JSON::PP::IncrParser->new;

    if ( $_[0]->{_incr_parser}->{incr_parsing} ) {
        Carp::croak("incr_text can not be called when the incremental parser already started parsing");
    }

    $_[0]->{_incr_parser}->{incr_text} = $_[1] if ( @_ > 1 );
    $_[0]->{_incr_parser}->{incr_text};
}


1;
__END__

=pod

=head1 NAME

JSON::PP5005 - Helper module in using JSON::PP in Perl 5.005

=head1 DESCRIPTION

JSON::PP calls internally.

=head1 AUTHOR

Makamaka Hannyaharamitu, E<lt>makamaka[at]cpan.orgE<gt>


=head1 COPYRIGHT AND LICENSE

Copyright 2007-2010 by Makamaka Hannyaharamitu

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut

