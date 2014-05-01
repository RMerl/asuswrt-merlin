package JSON;


use strict;
use Carp ();
use base qw(Exporter);
@JSON::EXPORT = qw(from_json to_json jsonToObj objToJson encode_json decode_json);

BEGIN {
    $JSON::VERSION = '2.53';
    $JSON::DEBUG   = 0 unless (defined $JSON::DEBUG);
    $JSON::DEBUG   = $ENV{ PERL_JSON_DEBUG } if exists $ENV{ PERL_JSON_DEBUG };
}

my $Module_XS  = 'JSON::XS';
my $Module_PP  = 'JSON::PP';
my $Module_bp  = 'JSON::backportPP'; # included in JSON distribution
my $PP_Version = '2.27200';
my $XS_Version = '2.27';


# XS and PP common methods

my @PublicMethods = qw/
    ascii latin1 utf8 pretty indent space_before space_after relaxed canonical allow_nonref 
    allow_blessed convert_blessed filter_json_object filter_json_single_key_object 
    shrink max_depth max_size encode decode decode_prefix allow_unknown
/;

my @Properties = qw/
    ascii latin1 utf8 indent space_before space_after relaxed canonical allow_nonref
    allow_blessed convert_blessed shrink max_depth max_size allow_unknown
/;

my @XSOnlyMethods = qw//; # Currently nothing

my @PPOnlyMethods = qw/
    indent_length sort_by
    allow_singlequote allow_bignum loose allow_barekey escape_slash as_nonblessed
/; # JSON::PP specific


# used in _load_xs and _load_pp ($INSTALL_ONLY is not used currently)
my $_INSTALL_DONT_DIE  = 1; # When _load_xs fails to load XS, don't die.
my $_INSTALL_ONLY      = 2; # Don't call _set_methods()
my $_ALLOW_UNSUPPORTED = 0;
my $_UNIV_CONV_BLESSED = 0;
my $_USSING_bpPP       = 0;


# Check the environment variable to decide worker module. 

unless ($JSON::Backend) {
    $JSON::DEBUG and  Carp::carp("Check used worker module...");

    my $backend = exists $ENV{PERL_JSON_BACKEND} ? $ENV{PERL_JSON_BACKEND} : 1;

    if ($backend eq '1' or $backend =~ /JSON::XS\s*,\s*JSON::PP/) {
        _load_xs($_INSTALL_DONT_DIE) or _load_pp();
    }
    elsif ($backend eq '0' or $backend eq 'JSON::PP') {
        _load_pp();
    }
    elsif ($backend eq '2' or $backend eq 'JSON::XS') {
        _load_xs();
    }
    elsif ($backend eq 'JSON::backportPP') {
        $_USSING_bpPP = 1;
        _load_pp();
    }
    else {
        Carp::croak "The value of environmental variable 'PERL_JSON_BACKEND' is invalid.";
    }
}


sub import {
    my $pkg = shift;
    my @what_to_export;
    my $no_export;

    for my $tag (@_) {
        if ($tag eq '-support_by_pp') {
            if (!$_ALLOW_UNSUPPORTED++) {
                JSON::Backend::XS
                    ->support_by_pp(@PPOnlyMethods) if ($JSON::Backend eq $Module_XS);
            }
            next;
        }
        elsif ($tag eq '-no_export') {
            $no_export++, next;
        }
        elsif ( $tag eq '-convert_blessed_universally' ) {
            eval q|
                require B;
                *UNIVERSAL::TO_JSON = sub {
                    my $b_obj = B::svref_2object( $_[0] );
                    return    $b_obj->isa('B::HV') ? { %{ $_[0] } }
                            : $b_obj->isa('B::AV') ? [ @{ $_[0] } ]
                            : undef
                            ;
                }
            | if ( !$_UNIV_CONV_BLESSED++ );
            next;
        }
        push @what_to_export, $tag;
    }

    return if ($no_export);

    __PACKAGE__->export_to_level(1, $pkg, @what_to_export);
}


# OBSOLETED

sub jsonToObj {
    my $alternative = 'from_json';
    if (defined $_[0] and UNIVERSAL::isa($_[0], 'JSON')) {
        shift @_; $alternative = 'decode';
    }
    Carp::carp "'jsonToObj' will be obsoleted. Please use '$alternative' instead.";
    return JSON::from_json(@_);
};

sub objToJson {
    my $alternative = 'to_json';
    if (defined $_[0] and UNIVERSAL::isa($_[0], 'JSON')) {
        shift @_; $alternative = 'encode';
    }
    Carp::carp "'objToJson' will be obsoleted. Please use '$alternative' instead.";
    JSON::to_json(@_);
};


# INTERFACES

sub to_json ($@) {
    if (
        ref($_[0]) eq 'JSON'
        or (@_ > 2 and $_[0] eq 'JSON')
    ) {
        Carp::croak "to_json should not be called as a method.";
    }
    my $json = new JSON;

    if (@_ == 2 and ref $_[1] eq 'HASH') {
        my $opt  = $_[1];
        for my $method (keys %$opt) {
            $json->$method( $opt->{$method} );
        }
    }

    $json->encode($_[0]);
}


sub from_json ($@) {
    if ( ref($_[0]) eq 'JSON' or $_[0] eq 'JSON' ) {
        Carp::croak "from_json should not be called as a method.";
    }
    my $json = new JSON;

    if (@_ == 2 and ref $_[1] eq 'HASH') {
        my $opt  = $_[1];
        for my $method (keys %$opt) {
            $json->$method( $opt->{$method} );
        }
    }

    return $json->decode( $_[0] );
}


sub true  { $JSON::true  }

sub false { $JSON::false }

sub null  { undef; }


sub require_xs_version { $XS_Version; }

sub backend {
    my $proto = shift;
    $JSON::Backend;
}

#*module = *backend;


sub is_xs {
    return $_[0]->module eq $Module_XS;
}


sub is_pp {
    return not $_[0]->xs;
}


sub pureperl_only_methods { @PPOnlyMethods; }


sub property {
    my ($self, $name, $value) = @_;

    if (@_ == 1) {
        my %props;
        for $name (@Properties) {
            my $method = 'get_' . $name;
            if ($name eq 'max_size') {
                my $value = $self->$method();
                $props{$name} = $value == 1 ? 0 : $value;
                next;
            }
            $props{$name} = $self->$method();
        }
        return \%props;
    }
    elsif (@_ > 3) {
        Carp::croak('property() can take only the option within 2 arguments.');
    }
    elsif (@_ == 2) {
        if ( my $method = $self->can('get_' . $name) ) {
            if ($name eq 'max_size') {
                my $value = $self->$method();
                return $value == 1 ? 0 : $value;
            }
            $self->$method();
        }
    }
    else {
        $self->$name($value);
    }

}



# INTERNAL

sub _load_xs {
    my $opt = shift;

    $JSON::DEBUG and Carp::carp "Load $Module_XS.";

    # if called after install module, overload is disable.... why?
    JSON::Boolean::_overrride_overload($Module_XS);
    JSON::Boolean::_overrride_overload($Module_PP);

    eval qq|
        use $Module_XS $XS_Version ();
    |;

    if ($@) {
        if (defined $opt and $opt & $_INSTALL_DONT_DIE) {
            $JSON::DEBUG and Carp::carp "Can't load $Module_XS...($@)";
            return 0;
        }
        Carp::croak $@;
    }

    unless (defined $opt and $opt & $_INSTALL_ONLY) {
        _set_module( $JSON::Backend = $Module_XS );
        my $data = join("", <DATA>); # this code is from Jcode 2.xx.
        close(DATA);
        eval $data;
        JSON::Backend::XS->init;
    }

    return 1;
};


sub _load_pp {
    my $opt = shift;
    my $backend = $_USSING_bpPP ? $Module_bp : $Module_PP;

    $JSON::DEBUG and Carp::carp "Load $backend.";

    # if called after install module, overload is disable.... why?
    JSON::Boolean::_overrride_overload($Module_XS);
    JSON::Boolean::_overrride_overload($backend);

    if ( $_USSING_bpPP ) {
        eval qq| require $backend |;
    }
    else {
        eval qq| use $backend $PP_Version () |;
    }

    if ($@) {
        if ( $backend eq $Module_PP ) {
            $JSON::DEBUG and Carp::carp "Can't load $Module_PP ($@), so try to load $Module_bp";
            $_USSING_bpPP++;
            $backend = $Module_bp;
            JSON::Boolean::_overrride_overload($backend);
            local $^W; # if PP installed but invalid version, backportPP redifines methods.
            eval qq| require $Module_bp |;
        }
        Carp::croak $@ if $@;
    }

    unless (defined $opt and $opt & $_INSTALL_ONLY) {
        _set_module( $JSON::Backend = $Module_PP ); # even if backportPP, set $Backend with 'JSON::PP'
        JSON::Backend::PP->init;
    }
};


sub _set_module {
    return if defined $JSON::true;

    my $module = shift;

    local $^W;
    no strict qw(refs);

    $JSON::true  = ${"$module\::true"};
    $JSON::false = ${"$module\::false"};

    push @JSON::ISA, $module;
    push @{"$module\::Boolean::ISA"}, qw(JSON::Boolean);

    *{"JSON::is_bool"} = \&{"$module\::is_bool"};

    for my $method ($module eq $Module_XS ? @PPOnlyMethods : @XSOnlyMethods) {
        *{"JSON::$method"} = sub {
            Carp::carp("$method is not supported in $module.");
            $_[0];
        };
    }

    return 1;
}



#
# JSON Boolean
#

package JSON::Boolean;

my %Installed;

sub _overrride_overload {
    return if ($Installed{ $_[0] }++);

    my $boolean = $_[0] . '::Boolean';

    eval sprintf(q|
        package %s;
        use overload (
            '""' => sub { ${$_[0]} == 1 ? 'true' : 'false' },
            'eq' => sub {
                my ($obj, $op) = ref ($_[0]) ? ($_[0], $_[1]) : ($_[1], $_[0]);
                if ($op eq 'true' or $op eq 'false') {
                    return "$obj" eq 'true' ? 'true' eq $op : 'false' eq $op;
                }
                else {
                    return $obj ? 1 == $op : 0 == $op;
                }
            },
        );
    |, $boolean);

    if ($@) { Carp::croak $@; }

    return 1;
}


#
# Helper classes for Backend Module (PP)
#

package JSON::Backend::PP;

sub init {
    local $^W;
    no strict qw(refs); # this routine may be called after JSON::Backend::XS init was called.
    *{"JSON::decode_json"} = \&{"JSON::PP::decode_json"};
    *{"JSON::encode_json"} = \&{"JSON::PP::encode_json"};
    *{"JSON::PP::is_xs"}  = sub { 0 };
    *{"JSON::PP::is_pp"}  = sub { 1 };
    return 1;
}

#
# To save memory, the below lines are read only when XS backend is used.
#

package JSON;

1;
__DATA__


#
# Helper classes for Backend Module (XS)
#

package JSON::Backend::XS;

use constant INDENT_LENGTH_FLAG => 15 << 12;

use constant UNSUPPORTED_ENCODE_FLAG => {
    ESCAPE_SLASH      => 0x00000010,
    ALLOW_BIGNUM      => 0x00000020,
    AS_NONBLESSED     => 0x00000040,
    EXPANDED          => 0x10000000, # for developer's
};

use constant UNSUPPORTED_DECODE_FLAG => {
    LOOSE             => 0x00000001,
    ALLOW_BIGNUM      => 0x00000002,
    ALLOW_BAREKEY     => 0x00000004,
    ALLOW_SINGLEQUOTE => 0x00000008,
    EXPANDED          => 0x20000000, # for developer's
};


sub init {
    local $^W;
    no strict qw(refs);
    *{"JSON::decode_json"} = \&{"JSON::XS::decode_json"};
    *{"JSON::encode_json"} = \&{"JSON::XS::encode_json"};
    *{"JSON::XS::is_xs"}  = sub { 1 };
    *{"JSON::XS::is_pp"}  = sub { 0 };
    return 1;
}


sub support_by_pp {
    my ($class, @methods) = @_;

    local $^W;
    no strict qw(refs);

    my $JSON_XS_encode_orignal     = \&JSON::XS::encode;
    my $JSON_XS_decode_orignal     = \&JSON::XS::decode;
    my $JSON_XS_incr_parse_orignal = \&JSON::XS::incr_parse;

    *JSON::XS::decode     = \&JSON::Backend::XS::Supportable::_decode;
    *JSON::XS::encode     = \&JSON::Backend::XS::Supportable::_encode;
    *JSON::XS::incr_parse = \&JSON::Backend::XS::Supportable::_incr_parse;

    *{JSON::XS::_original_decode}     = $JSON_XS_decode_orignal;
    *{JSON::XS::_original_encode}     = $JSON_XS_encode_orignal;
    *{JSON::XS::_original_incr_parse} = $JSON_XS_incr_parse_orignal;

    push @JSON::Backend::XS::Supportable::ISA, 'JSON';

    my $pkg = 'JSON::Backend::XS::Supportable';

    *{JSON::new} = sub {
        my $proto = new JSON::XS; $$proto = 0;
        bless  $proto, $pkg;
    };


    for my $method (@methods) {
        my $flag = uc($method);
        my $type |= (UNSUPPORTED_ENCODE_FLAG->{$flag} || 0);
           $type |= (UNSUPPORTED_DECODE_FLAG->{$flag} || 0);

        next unless($type);

        $pkg->_make_unsupported_method($method => $type);
    }

    push @{"JSON::XS::Boolean::ISA"}, qw(JSON::PP::Boolean);
    push @{"JSON::PP::Boolean::ISA"}, qw(JSON::Boolean);

    $JSON::DEBUG and Carp::carp("set -support_by_pp mode.");

    return 1;
}




#
# Helper classes for XS
#

package JSON::Backend::XS::Supportable;

$Carp::Internal{'JSON::Backend::XS::Supportable'} = 1;

sub _make_unsupported_method {
    my ($pkg, $method, $type) = @_;

    local $^W;
    no strict qw(refs);

    *{"$pkg\::$method"} = sub {
        local $^W;
        if (defined $_[1] ? $_[1] : 1) {
            ${$_[0]} |= $type;
        }
        else {
            ${$_[0]} &= ~$type;
        }
        $_[0];
    };

    *{"$pkg\::get_$method"} = sub {
        ${$_[0]} & $type ? 1 : '';
    };

}


sub _set_for_pp {
    JSON::_load_pp( $_INSTALL_ONLY );

    my $type  = shift;
    my $pp    = new JSON::PP;
    my $prop = $_[0]->property;

    for my $name (keys %$prop) {
        $pp->$name( $prop->{$name} ? $prop->{$name} : 0 );
    }

    my $unsupported = $type eq 'encode' ? JSON::Backend::XS::UNSUPPORTED_ENCODE_FLAG
                                        : JSON::Backend::XS::UNSUPPORTED_DECODE_FLAG;
    my $flags       = ${$_[0]} || 0;

    for my $name (keys %$unsupported) {
        next if ($name eq 'EXPANDED'); # for developer's
        my $enable = ($flags & $unsupported->{$name}) ? 1 : 0;
        my $method = lc $name;
        $pp->$method($enable);
    }

    $pp->indent_length( $_[0]->get_indent_length );

    return $pp;
}

sub _encode { # using with PP encod
    if (${$_[0]}) {
        _set_for_pp('encode' => @_)->encode($_[1]);
    }
    else {
        $_[0]->_original_encode( $_[1] );
    }
}


sub _decode { # if unsupported-flag is set, use PP
    if (${$_[0]}) {
        _set_for_pp('decode' => @_)->decode($_[1]);
    }
    else {
        $_[0]->_original_decode( $_[1] );
    }
}


sub decode_prefix { # if unsupported-flag is set, use PP
    _set_for_pp('decode' => @_)->decode_prefix($_[1]);
}


sub _incr_parse {
    if (${$_[0]}) {
        _set_for_pp('decode' => @_)->incr_parse($_[1]);
    }
    else {
        $_[0]->_original_incr_parse( $_[1] );
    }
}


sub get_indent_length {
    ${$_[0]} << 4 >> 16;
}


sub indent_length {
    my $length = $_[1];

    if (!defined $length or $length > 15 or $length < 0) {
        Carp::carp "The acceptable range of indent_length() is 0 to 15.";
    }
    else {
        local $^W;
        $length <<= 12;
        ${$_[0]} &= ~ JSON::Backend::XS::INDENT_LENGTH_FLAG;
        ${$_[0]} |= $length;
        *JSON::XS::encode = \&JSON::Backend::XS::Supportable::_encode;
    }

    $_[0];
}


1;
__END__

=head1 NAME

JSON - JSON (JavaScript Object Notation) encoder/decoder

=head1 SYNOPSIS

 use JSON; # imports encode_json, decode_json, to_json and from_json.
 
 # simple and fast interfaces (expect/generate UTF-8)
 
 $utf8_encoded_json_text = encode_json $perl_hash_or_arrayref;
 $perl_hash_or_arrayref  = decode_json $utf8_encoded_json_text;
 
 # OO-interface
 
 $json = JSON->new->allow_nonref;
 
 $json_text   = $json->encode( $perl_scalar );
 $perl_scalar = $json->decode( $json_text );
 
 $pretty_printed = $json->pretty->encode( $perl_scalar ); # pretty-printing
 
 # If you want to use PP only support features, call with '-support_by_pp'
 # When XS unsupported feature is enable, using PP (de|en)code instead of XS ones.
 
 use JSON -support_by_pp;
 
 # option-acceptable interfaces (expect/generate UNICODE by default)
 
 $json_text   = to_json( $perl_scalar, { ascii => 1, pretty => 1 } );
 $perl_scalar = from_json( $json_text, { utf8  => 1 } );
 
 # Between (en|de)code_json and (to|from)_json, if you want to write
 # a code which communicates to an outer world (encoded in UTF-8),
 # recommend to use (en|de)code_json.
 
=head1 VERSION

    2.53

This version is compatible with JSON::XS B<2.27> and later.


=head1 NOTE

JSON::PP was inculded in C<JSON> distribution.
It comes to be a perl core module in Perl 5.14.
And L<JSON::PP> will be split away it.

C<JSON> distribution will inculde yet another JSON::PP modules.
They are JSNO::backportPP and so on. JSON.pm should work as it did at all.

=head1 DESCRIPTION

 ************************** CAUTION ********************************
 * This is 'JSON module version 2' and there are many differences  *
 * to version 1.xx                                                 *
 * Please check your applications useing old version.              *
 *   See to 'INCOMPATIBLE CHANGES TO OLD VERSION'                  *
 *******************************************************************

JSON (JavaScript Object Notation) is a simple data format.
See to L<http://www.json.org/> and C<RFC4627>(L<http://www.ietf.org/rfc/rfc4627.txt>).

This module converts Perl data structures to JSON and vice versa using either
L<JSON::XS> or L<JSON::PP>.

JSON::XS is the fastest and most proper JSON module on CPAN which must be
compiled and installed in your environment.
JSON::PP is a pure-Perl module which is bundled in this distribution and
has a strong compatibility to JSON::XS.

This module try to use JSON::XS by default and fail to it, use JSON::PP instead.
So its features completely depend on JSON::XS or JSON::PP.

See to L<BACKEND MODULE DECISION>.

To distinguish the module name 'JSON' and the format type JSON,
the former is quoted by CE<lt>E<gt> (its results vary with your using media),
and the latter is left just as it is.

Module name : C<JSON>

Format type : JSON

=head2 FEATURES

=over

=item * correct unicode handling

This module (i.e. backend modules) knows how to handle Unicode, documents
how and when it does so, and even documents what "correct" means.

Even though there are limitations, this feature is available since Perl version 5.6.

JSON::XS requires Perl 5.8.2 (but works correctly in 5.8.8 or later), so in older versions
C<JSON> sholud call JSON::PP as the backend which can be used since Perl 5.005.

With Perl 5.8.x JSON::PP works, but from 5.8.0 to 5.8.2, because of a Perl side problem,
JSON::PP works slower in the versions. And in 5.005, the Unicode handling is not available.
See to L<JSON::PP/UNICODE HANDLING ON PERLS> for more information.

See also to L<JSON::XS/A FEW NOTES ON UNICODE AND PERL>
and L<JSON::XS/ENCODING/CODESET_FLAG_NOTES>.


=item * round-trip integrity

When you serialise a perl data structure using only data types supported
by JSON and Perl, the deserialised data structure is identical on the Perl
level. (e.g. the string "2.0" doesn't suddenly become "2" just because
it looks like a number). There I<are> minor exceptions to this, read the
L</MAPPING> section below to learn about those.


=item * strict checking of JSON correctness

There is no guessing, no generating of illegal JSON texts by default,
and only JSON is accepted as input by default (the latter is a security
feature).

See to L<JSON::XS/FEATURES> and L<JSON::PP/FEATURES>.

=item * fast

This module returns a JSON::XS object itself if available.
Compared to other JSON modules and other serialisers such as Storable,
JSON::XS usually compares favourably in terms of speed, too.

If not available, C<JSON> returns a JSON::PP object instead of JSON::XS and
it is very slow as pure-Perl.

=item * simple to use

This module has both a simple functional interface as well as an
object oriented interface interface.

=item * reasonably versatile output formats

You can choose between the most compact guaranteed-single-line format possible
(nice for simple line-based protocols), a pure-ASCII format (for when your transport
is not 8-bit clean, still supports the whole Unicode range), or a pretty-printed
format (for when you want to read that stuff). Or you can combine those features
in whatever way you like.

=back

=head1 FUNCTIONAL INTERFACE

Some documents are copied and modified from L<JSON::XS/FUNCTIONAL INTERFACE>.
C<to_json> and C<from_json> are additional functions.

=head2 encode_json

    $json_text = encode_json $perl_scalar

Converts the given Perl data structure to a UTF-8 encoded, binary string.

This function call is functionally identical to:

    $json_text = JSON->new->utf8->encode($perl_scalar)

=head2 decode_json

    $perl_scalar = decode_json $json_text

The opposite of C<encode_json>: expects an UTF-8 (binary) string and tries
to parse that as an UTF-8 encoded JSON text, returning the resulting
reference.

This function call is functionally identical to:

    $perl_scalar = JSON->new->utf8->decode($json_text)


=head2 to_json

   $json_text = to_json($perl_scalar)

Converts the given Perl data structure to a json string.

This function call is functionally identical to:

   $json_text = JSON->new->encode($perl_scalar)

Takes a hash reference as the second.

   $json_text = to_json($perl_scalar, $flag_hashref)

So,

   $json_text = to_json($perl_scalar, {utf8 => 1, pretty => 1})

equivalent to:

   $json_text = JSON->new->utf8(1)->pretty(1)->encode($perl_scalar)

If you want to write a modern perl code which communicates to outer world,
you should use C<encode_json> (supposed that JSON data are encoded in UTF-8).

=head2 from_json

   $perl_scalar = from_json($json_text)

The opposite of C<to_json>: expects a json string and tries
to parse it, returning the resulting reference.

This function call is functionally identical to:

    $perl_scalar = JSON->decode($json_text)

Takes a hash reference as the second.

    $perl_scalar = from_json($json_text, $flag_hashref)

So,

    $perl_scalar = from_json($json_text, {utf8 => 1})

equivalent to:

    $perl_scalar = JSON->new->utf8(1)->decode($json_text)

If you want to write a modern perl code which communicates to outer world,
you should use C<decode_json> (supposed that JSON data are encoded in UTF-8).

=head2 JSON::is_bool

    $is_boolean = JSON::is_bool($scalar)

Returns true if the passed scalar represents either JSON::true or
JSON::false, two constants that act like C<1> and C<0> respectively
and are also used to represent JSON C<true> and C<false> in Perl strings.

=head2 JSON::true

Returns JSON true value which is blessed object.
It C<isa> JSON::Boolean object.

=head2 JSON::false

Returns JSON false value which is blessed object.
It C<isa> JSON::Boolean object.

=head2 JSON::null

Returns C<undef>.

See L<MAPPING>, below, for more information on how JSON values are mapped to
Perl.

=head1 HOW DO I DECODE A DATA FROM OUTER AND ENCODE TO OUTER

This section supposes that your perl vresion is 5.8 or later.

If you know a JSON text from an outer world - a network, a file content, and so on,
is encoded in UTF-8, you should use C<decode_json> or C<JSON> module object
with C<utf8> enable. And the decoded result will contain UNICODE characters.

  # from network
  my $json        = JSON->new->utf8;
  my $json_text   = CGI->new->param( 'json_data' );
  my $perl_scalar = $json->decode( $json_text );
  
  # from file content
  local $/;
  open( my $fh, '<', 'json.data' );
  $json_text   = <$fh>;
  $perl_scalar = decode_json( $json_text );

If an outer data is not encoded in UTF-8, firstly you should C<decode> it.

  use Encode;
  local $/;
  open( my $fh, '<', 'json.data' );
  my $encoding = 'cp932';
  my $unicode_json_text = decode( $encoding, <$fh> ); # UNICODE
  
  # or you can write the below code.
  #
  # open( my $fh, "<:encoding($encoding)", 'json.data' );
  # $unicode_json_text = <$fh>;

In this case, C<$unicode_json_text> is of course UNICODE string.
So you B<cannot> use C<decode_json> nor C<JSON> module object with C<utf8> enable.
Instead of them, you use C<JSON> module object with C<utf8> disable or C<from_json>.

  $perl_scalar = $json->utf8(0)->decode( $unicode_json_text );
  # or
  $perl_scalar = from_json( $unicode_json_text );

Or C<encode 'utf8'> and C<decode_json>:

  $perl_scalar = decode_json( encode( 'utf8', $unicode_json_text ) );
  # this way is not efficient.

And now, you want to convert your C<$perl_scalar> into JSON data and
send it to an outer world - a network or a file content, and so on.

Your data usually contains UNICODE strings and you want the converted data to be encoded
in UTF-8, you should use C<encode_json> or C<JSON> module object with C<utf8> enable.

  print encode_json( $perl_scalar ); # to a network? file? or display?
  # or
  print $json->utf8->encode( $perl_scalar );

If C<$perl_scalar> does not contain UNICODE but C<$encoding>-encoded strings
for some reason, then its characters are regarded as B<latin1> for perl
(because it does not concern with your $encoding).
You B<cannot> use C<encode_json> nor C<JSON> module object with C<utf8> enable.
Instead of them, you use C<JSON> module object with C<utf8> disable or C<to_json>.
Note that the resulted text is a UNICODE string but no problem to print it.

  # $perl_scalar contains $encoding encoded string values
  $unicode_json_text = $json->utf8(0)->encode( $perl_scalar );
  # or 
  $unicode_json_text = to_json( $perl_scalar );
  # $unicode_json_text consists of characters less than 0x100
  print $unicode_json_text;

Or C<decode $encoding> all string values and C<encode_json>:

  $perl_scalar->{ foo } = decode( $encoding, $perl_scalar->{ foo } );
  # ... do it to each string values, then encode_json
  $json_text = encode_json( $perl_scalar );

This method is a proper way but probably not efficient.

See to L<Encode>, L<perluniintro>.


=head1 COMMON OBJECT-ORIENTED INTERFACE

=head2 new

    $json = new JSON

Returns a new C<JSON> object inherited from either JSON::XS or JSON::PP
that can be used to de/encode JSON strings.

All boolean flags described below are by default I<disabled>.

The mutators for flags all return the JSON object again and thus calls can
be chained:

   my $json = JSON->new->utf8->space_after->encode({a => [1,2]})
   => {"a": [1, 2]}

=head2 ascii

    $json = $json->ascii([$enable])
    
    $enabled = $json->get_ascii

If $enable is true (or missing), then the encode method will not generate characters outside
the code range 0..127. Any Unicode characters outside that range will be escaped using either
a single \uXXXX or a double \uHHHH\uLLLLL escape sequence, as per RFC4627.

If $enable is false, then the encode method will not escape Unicode characters unless
required by the JSON syntax or other flags. This results in a faster and more compact format.

This feature depends on the used Perl version and environment.

See to L<JSON::PP/UNICODE HANDLING ON PERLS> if the backend is PP.

  JSON->new->ascii(1)->encode([chr 0x10401])
  => ["\ud801\udc01"]

=head2 latin1

    $json = $json->latin1([$enable])
    
    $enabled = $json->get_latin1

If $enable is true (or missing), then the encode method will encode the resulting JSON
text as latin1 (or iso-8859-1), escaping any characters outside the code range 0..255.

If $enable is false, then the encode method will not escape Unicode characters
unless required by the JSON syntax or other flags.

  JSON->new->latin1->encode (["\x{89}\x{abc}"]
  => ["\x{89}\\u0abc"]    # (perl syntax, U+abc escaped, U+89 not)

=head2 utf8

    $json = $json->utf8([$enable])
    
    $enabled = $json->get_utf8

If $enable is true (or missing), then the encode method will encode the JSON result
into UTF-8, as required by many protocols, while the decode method expects to be handled
an UTF-8-encoded string. Please note that UTF-8-encoded strings do not contain any
characters outside the range 0..255, they are thus useful for bytewise/binary I/O.

In future versions, enabling this option might enable autodetection of the UTF-16 and UTF-32
encoding families, as described in RFC4627.

If $enable is false, then the encode method will return the JSON string as a (non-encoded)
Unicode string, while decode expects thus a Unicode string. Any decoding or encoding
(e.g. to UTF-8 or UTF-16) needs to be done yourself, e.g. using the Encode module.


Example, output UTF-16BE-encoded JSON:

  use Encode;
  $jsontext = encode "UTF-16BE", JSON::XS->new->encode ($object);

Example, decode UTF-32LE-encoded JSON:

  use Encode;
  $object = JSON::XS->new->decode (decode "UTF-32LE", $jsontext);

See to L<JSON::PP/UNICODE HANDLING ON PERLS> if the backend is PP.


=head2 pretty

    $json = $json->pretty([$enable])

This enables (or disables) all of the C<indent>, C<space_before> and
C<space_after> (and in the future possibly more) flags in one call to
generate the most readable (or most compact) form possible.

Equivalent to:

   $json->indent->space_before->space_after

The indent space length is three and JSON::XS cannot change the indent
space length.

=head2 indent

    $json = $json->indent([$enable])
    
    $enabled = $json->get_indent

If C<$enable> is true (or missing), then the C<encode> method will use a multiline
format as output, putting every array member or object/hash key-value pair
into its own line, identing them properly.

If C<$enable> is false, no newlines or indenting will be produced, and the
resulting JSON text is guarenteed not to contain any C<newlines>.

This setting has no effect when decoding JSON texts.

The indent space length is three.
With JSON::PP, you can also access C<indent_length> to change indent space length.


=head2 space_before

    $json = $json->space_before([$enable])
    
    $enabled = $json->get_space_before

If C<$enable> is true (or missing), then the C<encode> method will add an extra
optional space before the C<:> separating keys from values in JSON objects.

If C<$enable> is false, then the C<encode> method will not add any extra
space at those places.

This setting has no effect when decoding JSON texts.

Example, space_before enabled, space_after and indent disabled:

   {"key" :"value"}


=head2 space_after

    $json = $json->space_after([$enable])
    
    $enabled = $json->get_space_after

If C<$enable> is true (or missing), then the C<encode> method will add an extra
optional space after the C<:> separating keys from values in JSON objects
and extra whitespace after the C<,> separating key-value pairs and array
members.

If C<$enable> is false, then the C<encode> method will not add any extra
space at those places.

This setting has no effect when decoding JSON texts.

Example, space_before and indent disabled, space_after enabled:

   {"key": "value"}


=head2 relaxed

    $json = $json->relaxed([$enable])
    
    $enabled = $json->get_relaxed

If C<$enable> is true (or missing), then C<decode> will accept some
extensions to normal JSON syntax (see below). C<encode> will not be
affected in anyway. I<Be aware that this option makes you accept invalid
JSON texts as if they were valid!>. I suggest only to use this option to
parse application-specific files written by humans (configuration files,
resource files etc.)

If C<$enable> is false (the default), then C<decode> will only accept
valid JSON texts.

Currently accepted extensions are:

=over 4

=item * list items can have an end-comma

JSON I<separates> array elements and key-value pairs with commas. This
can be annoying if you write JSON texts manually and want to be able to
quickly append elements, so this extension accepts comma at the end of
such items not just between them:

   [
      1,
      2, <- this comma not normally allowed
   ]
   {
      "k1": "v1",
      "k2": "v2", <- this comma not normally allowed
   }

=item * shell-style '#'-comments

Whenever JSON allows whitespace, shell-style comments are additionally
allowed. They are terminated by the first carriage-return or line-feed
character, after which more white-space and comments are allowed.

  [
     1, # this comment not allowed in JSON
        # neither this one...
  ]

=back


=head2 canonical

    $json = $json->canonical([$enable])
    
    $enabled = $json->get_canonical

If C<$enable> is true (or missing), then the C<encode> method will output JSON objects
by sorting their keys. This is adding a comparatively high overhead.

If C<$enable> is false, then the C<encode> method will output key-value
pairs in the order Perl stores them (which will likely change between runs
of the same script).

This option is useful if you want the same data structure to be encoded as
the same JSON text (given the same overall settings). If it is disabled,
the same hash might be encoded differently even if contains the same data,
as key-value pairs have no inherent ordering in Perl.

This setting has no effect when decoding JSON texts.

=head2 allow_nonref

    $json = $json->allow_nonref([$enable])
    
    $enabled = $json->get_allow_nonref

If C<$enable> is true (or missing), then the C<encode> method can convert a
non-reference into its corresponding string, number or null JSON value,
which is an extension to RFC4627. Likewise, C<decode> will accept those JSON
values instead of croaking.

If C<$enable> is false, then the C<encode> method will croak if it isn't
passed an arrayref or hashref, as JSON texts must either be an object
or array. Likewise, C<decode> will croak if given something that is not a
JSON object or array.

   JSON->new->allow_nonref->encode ("Hello, World!")
   => "Hello, World!"

=head2 allow_unknown

    $json = $json->allow_unknown ([$enable])
    
    $enabled = $json->get_allow_unknown

If $enable is true (or missing), then "encode" will *not* throw an
exception when it encounters values it cannot represent in JSON (for
example, filehandles) but instead will encode a JSON "null" value.
Note that blessed objects are not included here and are handled
separately by c<allow_nonref>.

If $enable is false (the default), then "encode" will throw an
exception when it encounters anything it cannot encode as JSON.

This option does not affect "decode" in any way, and it is
recommended to leave it off unless you know your communications
partner.

=head2 allow_blessed

    $json = $json->allow_blessed([$enable])
    
    $enabled = $json->get_allow_blessed

If C<$enable> is true (or missing), then the C<encode> method will not
barf when it encounters a blessed reference. Instead, the value of the
B<convert_blessed> option will decide whether C<null> (C<convert_blessed>
disabled or no C<TO_JSON> method found) or a representation of the
object (C<convert_blessed> enabled and C<TO_JSON> method found) is being
encoded. Has no effect on C<decode>.

If C<$enable> is false (the default), then C<encode> will throw an
exception when it encounters a blessed object.


=head2 convert_blessed

    $json = $json->convert_blessed([$enable])
    
    $enabled = $json->get_convert_blessed

If C<$enable> is true (or missing), then C<encode>, upon encountering a
blessed object, will check for the availability of the C<TO_JSON> method
on the object's class. If found, it will be called in scalar context
and the resulting scalar will be encoded instead of the object. If no
C<TO_JSON> method is found, the value of C<allow_blessed> will decide what
to do.

The C<TO_JSON> method may safely call die if it wants. If C<TO_JSON>
returns other blessed objects, those will be handled in the same
way. C<TO_JSON> must take care of not causing an endless recursion cycle
(== crash) in this case. The name of C<TO_JSON> was chosen because other
methods called by the Perl core (== not by the user of the object) are
usually in upper case letters and to avoid collisions with the C<to_json>
function or method.

This setting does not yet influence C<decode> in any way.

If C<$enable> is false, then the C<allow_blessed> setting will decide what
to do when a blessed object is found.

=over

=item convert_blessed_universally mode

If use C<JSON> with C<-convert_blessed_universally>, the C<UNIVERSAL::TO_JSON>
subroutine is defined as the below code:

   *UNIVERSAL::TO_JSON = sub {
       my $b_obj = B::svref_2object( $_[0] );
       return    $b_obj->isa('B::HV') ? { %{ $_[0] } }
               : $b_obj->isa('B::AV') ? [ @{ $_[0] } ]
               : undef
               ;
   }

This will cause that C<encode> method converts simple blessed objects into
JSON objects as non-blessed object.

   JSON -convert_blessed_universally;
   $json->allow_blessed->convert_blessed->encode( $blessed_object )

This feature is experimental and may be removed in the future.

=back

=head2 filter_json_object

    $json = $json->filter_json_object([$coderef])

When C<$coderef> is specified, it will be called from C<decode> each
time it decodes a JSON object. The only argument passed to the coderef
is a reference to the newly-created hash. If the code references returns
a single scalar (which need not be a reference), this value
(i.e. a copy of that scalar to avoid aliasing) is inserted into the
deserialised data structure. If it returns an empty list
(NOTE: I<not> C<undef>, which is a valid scalar), the original deserialised
hash will be inserted. This setting can slow down decoding considerably.

When C<$coderef> is omitted or undefined, any existing callback will
be removed and C<decode> will not change the deserialised hash in any
way.

Example, convert all JSON objects into the integer 5:

   my $js = JSON->new->filter_json_object (sub { 5 });
   # returns [5]
   $js->decode ('[{}]'); # the given subroutine takes a hash reference.
   # throw an exception because allow_nonref is not enabled
   # so a lone 5 is not allowed.
   $js->decode ('{"a":1, "b":2}');


=head2 filter_json_single_key_object

    $json = $json->filter_json_single_key_object($key [=> $coderef])

Works remotely similar to C<filter_json_object>, but is only called for
JSON objects having a single key named C<$key>.

This C<$coderef> is called before the one specified via
C<filter_json_object>, if any. It gets passed the single value in the JSON
object. If it returns a single value, it will be inserted into the data
structure. If it returns nothing (not even C<undef> but the empty list),
the callback from C<filter_json_object> will be called next, as if no
single-key callback were specified.

If C<$coderef> is omitted or undefined, the corresponding callback will be
disabled. There can only ever be one callback for a given key.

As this callback gets called less often then the C<filter_json_object>
one, decoding speed will not usually suffer as much. Therefore, single-key
objects make excellent targets to serialise Perl objects into, especially
as single-key JSON objects are as close to the type-tagged value concept
as JSON gets (it's basically an ID/VALUE tuple). Of course, JSON does not
support this in any way, so you need to make sure your data never looks
like a serialised Perl hash.

Typical names for the single object key are C<__class_whatever__>, or
C<$__dollars_are_rarely_used__$> or C<}ugly_brace_placement>, or even
things like C<__class_md5sum(classname)__>, to reduce the risk of clashing
with real hashes.

Example, decode JSON objects of the form C<< { "__widget__" => <id> } >>
into the corresponding C<< $WIDGET{<id>} >> object:

   # return whatever is in $WIDGET{5}:
   JSON
      ->new
      ->filter_json_single_key_object (__widget__ => sub {
            $WIDGET{ $_[0] }
         })
      ->decode ('{"__widget__": 5')

   # this can be used with a TO_JSON method in some "widget" class
   # for serialisation to json:
   sub WidgetBase::TO_JSON {
      my ($self) = @_;

      unless ($self->{id}) {
         $self->{id} = ..get..some..id..;
         $WIDGET{$self->{id}} = $self;
      }

      { __widget__ => $self->{id} }
   }


=head2 shrink

    $json = $json->shrink([$enable])
    
    $enabled = $json->get_shrink

With JSON::XS, this flag resizes strings generated by either
C<encode> or C<decode> to their minimum size possible. This can save
memory when your JSON texts are either very very long or you have many
short strings. It will also try to downgrade any strings to octet-form
if possible: perl stores strings internally either in an encoding called
UTF-X or in octet-form. The latter cannot store everything but uses less
space in general (and some buggy Perl or C code might even rely on that
internal representation being used).

With JSON::PP, it is noop about resizing strings but tries
C<utf8::downgrade> to the returned string by C<encode>. See to L<utf8>.

See to L<JSON::XS/OBJECT-ORIENTED INTERFACE> and L<JSON::PP/METHODS>.

=head2 max_depth

    $json = $json->max_depth([$maximum_nesting_depth])
    
    $max_depth = $json->get_max_depth

Sets the maximum nesting level (default C<512>) accepted while encoding
or decoding. If a higher nesting level is detected in JSON text or a Perl
data structure, then the encoder and decoder will stop and croak at that
point.

Nesting level is defined by number of hash- or arrayrefs that the encoder
needs to traverse to reach a given point or the number of C<{> or C<[>
characters without their matching closing parenthesis crossed to reach a
given character in a string.

If no argument is given, the highest possible setting will be used, which
is rarely useful.

Note that nesting is implemented by recursion in C. The default value has
been chosen to be as large as typical operating systems allow without
crashing. (JSON::XS)

With JSON::PP as the backend, when a large value (100 or more) was set and
it de/encodes a deep nested object/text, it may raise a warning
'Deep recursion on subroutin' at the perl runtime phase.

See L<JSON::XS/SECURITY CONSIDERATIONS> for more info on why this is useful.

=head2 max_size

    $json = $json->max_size([$maximum_string_size])
    
    $max_size = $json->get_max_size

Set the maximum length a JSON text may have (in bytes) where decoding is
being attempted. The default is C<0>, meaning no limit. When C<decode>
is called on a string that is longer then this many bytes, it will not
attempt to decode the string but throw an exception. This setting has no
effect on C<encode> (yet).

If no argument is given, the limit check will be deactivated (same as when
C<0> is specified).

See L<JSON::XS/SECURITY CONSIDERATIONS>, below, for more info on why this is useful.

=head2 encode

    $json_text = $json->encode($perl_scalar)

Converts the given Perl data structure (a simple scalar or a reference
to a hash or array) to its JSON representation. Simple scalars will be
converted into JSON string or number sequences, while references to arrays
become JSON arrays and references to hashes become JSON objects. Undefined
Perl values (e.g. C<undef>) become JSON C<null> values.
References to the integers C<0> and C<1> are converted into C<true> and C<false>.

=head2 decode

    $perl_scalar = $json->decode($json_text)

The opposite of C<encode>: expects a JSON text and tries to parse it,
returning the resulting simple scalar or reference. Croaks on error.

JSON numbers and strings become simple Perl scalars. JSON arrays become
Perl arrayrefs and JSON objects become Perl hashrefs. C<true> becomes
C<1> (C<JSON::true>), C<false> becomes C<0> (C<JSON::false>) and
C<null> becomes C<undef>.

=head2 decode_prefix

    ($perl_scalar, $characters) = $json->decode_prefix($json_text)

This works like the C<decode> method, but instead of raising an exception
when there is trailing garbage after the first JSON object, it will
silently stop parsing there and return the number of characters consumed
so far.

   JSON->new->decode_prefix ("[1] the tail")
   => ([], 3)

See to L<JSON::XS/OBJECT-ORIENTED INTERFACE>

=head2 property

    $boolean = $json->property($property_name)

Returns a boolean value about above some properties.

The available properties are C<ascii>, C<latin1>, C<utf8>,
C<indent>,C<space_before>, C<space_after>, C<relaxed>, C<canonical>,
C<allow_nonref>, C<allow_unknown>, C<allow_blessed>, C<convert_blessed>,
C<shrink>, C<max_depth> and C<max_size>.

   $boolean = $json->property('utf8');
    => 0
   $json->utf8;
   $boolean = $json->property('utf8');
    => 1

Sets the property with a given boolean value.

    $json = $json->property($property_name => $boolean);

With no argumnt, it returns all the above properties as a hash reference.

    $flag_hashref = $json->property();

=head1 INCREMENTAL PARSING

Most of this section are copied and modified from L<JSON::XS/INCREMENTAL PARSING>.

In some cases, there is the need for incremental parsing of JSON texts.
This module does allow you to parse a JSON stream incrementally.
It does so by accumulating text until it has a full JSON object, which
it then can decode. This process is similar to using C<decode_prefix>
to see if a full JSON object is available, but is much more efficient
(and can be implemented with a minimum of method calls).

The backend module will only attempt to parse the JSON text once it is sure it
has enough text to get a decisive result, using a very simple but
truly incremental parser. This means that it sometimes won't stop as
early as the full parser, for example, it doesn't detect parenthese
mismatches. The only thing it guarantees is that it starts decoding as
soon as a syntactically valid JSON text has been seen. This means you need
to set resource limits (e.g. C<max_size>) to ensure the parser will stop
parsing in the presence if syntax errors.

The following methods implement this incremental parser.

=head2 incr_parse

    $json->incr_parse( [$string] ) # void context
    
    $obj_or_undef = $json->incr_parse( [$string] ) # scalar context
    
    @obj_or_empty = $json->incr_parse( [$string] ) # list context

This is the central parsing function. It can both append new text and
extract objects from the stream accumulated so far (both of these
functions are optional).

If C<$string> is given, then this string is appended to the already
existing JSON fragment stored in the C<$json> object.

After that, if the function is called in void context, it will simply
return without doing anything further. This can be used to add more text
in as many chunks as you want.

If the method is called in scalar context, then it will try to extract
exactly I<one> JSON object. If that is successful, it will return this
object, otherwise it will return C<undef>. If there is a parse error,
this method will croak just as C<decode> would do (one can then use
C<incr_skip> to skip the errornous part). This is the most common way of
using the method.

And finally, in list context, it will try to extract as many objects
from the stream as it can find and return them, or the empty list
otherwise. For this to work, there must be no separators between the JSON
objects or arrays, instead they must be concatenated back-to-back. If
an error occurs, an exception will be raised as in the scalar context
case. Note that in this case, any previously-parsed JSON texts will be
lost.

Example: Parse some JSON arrays/objects in a given string and return them.

    my @objs = JSON->new->incr_parse ("[5][7][1,2]");

=head2 incr_text

    $lvalue_string = $json->incr_text

This method returns the currently stored JSON fragment as an lvalue, that
is, you can manipulate it. This I<only> works when a preceding call to
C<incr_parse> in I<scalar context> successfully returned an object. Under
all other circumstances you must not call this function (I mean it.
although in simple tests it might actually work, it I<will> fail under
real world conditions). As a special exception, you can also call this
method before having parsed anything.

This function is useful in two cases: a) finding the trailing text after a
JSON object or b) parsing multiple JSON objects separated by non-JSON text
(such as commas).

    $json->incr_text =~ s/\s*,\s*//;

In Perl 5.005, C<lvalue> attribute is not available.
You must write codes like the below:

    $string = $json->incr_text;
    $string =~ s/\s*,\s*//;
    $json->incr_text( $string );

=head2 incr_skip

    $json->incr_skip

This will reset the state of the incremental parser and will remove the
parsed text from the input buffer. This is useful after C<incr_parse>
died, in which case the input buffer and incremental parser state is left
unchanged, to skip the text parsed so far and to reset the parse state.

=head2 incr_reset

    $json->incr_reset

This completely resets the incremental parser, that is, after this call,
it will be as if the parser had never parsed anything.

This is useful if you want ot repeatedly parse JSON objects and want to
ignore any trailing data, which means you have to reset the parser after
each successful decode.

See to L<JSON::XS/INCREMENTAL PARSING> for examples.


=head1 JSON::PP SUPPORT METHODS

The below methods are JSON::PP own methods, so when C<JSON> works
with JSON::PP (i.e. the created object is a JSON::PP object), available.
See to L<JSON::PP/JSON::PP OWN METHODS> in detail.

If you use C<JSON> with additonal C<-support_by_pp>, some methods
are available even with JSON::XS. See to L<USE PP FEATURES EVEN THOUGH XS BACKEND>.

   BEING { $ENV{PERL_JSON_BACKEND} = 'JSON::XS' }
   
   use JSON -support_by_pp;
   
   my $json = new JSON;
   $json->allow_nonref->escape_slash->encode("/");

   # functional interfaces too.
   print to_json(["/"], {escape_slash => 1});
   print from_json('["foo"]', {utf8 => 1});

If you do not want to all functions but C<-support_by_pp>,
use C<-no_export>.

   use JSON -support_by_pp, -no_export;
   # functional interfaces are not exported.

=head2 allow_singlequote

    $json = $json->allow_singlequote([$enable])

If C<$enable> is true (or missing), then C<decode> will accept
any JSON strings quoted by single quotations that are invalid JSON
format.

    $json->allow_singlequote->decode({"foo":'bar'});
    $json->allow_singlequote->decode({'foo':"bar"});
    $json->allow_singlequote->decode({'foo':'bar'});

As same as the C<relaxed> option, this option may be used to parse
application-specific files written by humans.

=head2 allow_barekey

    $json = $json->allow_barekey([$enable])

If C<$enable> is true (or missing), then C<decode> will accept
bare keys of JSON object that are invalid JSON format.

As same as the C<relaxed> option, this option may be used to parse
application-specific files written by humans.

    $json->allow_barekey->decode('{foo:"bar"}');

=head2 allow_bignum

    $json = $json->allow_bignum([$enable])

If C<$enable> is true (or missing), then C<decode> will convert
the big integer Perl cannot handle as integer into a L<Math::BigInt>
object and convert a floating number (any) into a L<Math::BigFloat>.

On the contary, C<encode> converts C<Math::BigInt> objects and C<Math::BigFloat>
objects into JSON numbers with C<allow_blessed> enable.

   $json->allow_nonref->allow_blessed->allow_bignum;
   $bigfloat = $json->decode('2.000000000000000000000000001');
   print $json->encode($bigfloat);
   # => 2.000000000000000000000000001

See to L<MAPPING> aboout the conversion of JSON number.

=head2 loose

    $json = $json->loose([$enable])

The unescaped [\x00-\x1f\x22\x2f\x5c] strings are invalid in JSON strings
and the module doesn't allow to C<decode> to these (except for \x2f).
If C<$enable> is true (or missing), then C<decode>  will accept these
unescaped strings.

    $json->loose->decode(qq|["abc
                                   def"]|);

See to L<JSON::PP/JSON::PP OWN METHODS>.

=head2 escape_slash

    $json = $json->escape_slash([$enable])

According to JSON Grammar, I<slash> (U+002F) is escaped. But by default
JSON backend modules encode strings without escaping slash.

If C<$enable> is true (or missing), then C<encode> will escape slashes.

=head2 indent_length

    $json = $json->indent_length($length)

With JSON::XS, The indent space length is 3 and cannot be changed.
With JSON::PP, it sets the indent space length with the given $length.
The default is 3. The acceptable range is 0 to 15.

=head2 sort_by

    $json = $json->sort_by($function_name)
    $json = $json->sort_by($subroutine_ref)

If $function_name or $subroutine_ref are set, its sort routine are used.

   $js = $pc->sort_by(sub { $JSON::PP::a cmp $JSON::PP::b })->encode($obj);
   # is($js, q|{"a":1,"b":2,"c":3,"d":4,"e":5,"f":6,"g":7,"h":8,"i":9}|);

   $js = $pc->sort_by('own_sort')->encode($obj);
   # is($js, q|{"a":1,"b":2,"c":3,"d":4,"e":5,"f":6,"g":7,"h":8,"i":9}|);

   sub JSON::PP::own_sort { $JSON::PP::a cmp $JSON::PP::b }

As the sorting routine runs in the JSON::PP scope, the given
subroutine name and the special variables C<$a>, C<$b> will begin
with 'JSON::PP::'.

If $integer is set, then the effect is same as C<canonical> on.

See to L<JSON::PP/JSON::PP OWN METHODS>.

=head1 MAPPING

This section is copied from JSON::XS and modified to C<JSON>.
JSON::XS and JSON::PP mapping mechanisms are almost equivalent.

See to L<JSON::XS/MAPPING>.

=head2 JSON -> PERL

=over 4

=item object

A JSON object becomes a reference to a hash in Perl. No ordering of object
keys is preserved (JSON does not preserver object key ordering itself).

=item array

A JSON array becomes a reference to an array in Perl.

=item string

A JSON string becomes a string scalar in Perl - Unicode codepoints in JSON
are represented by the same codepoints in the Perl string, so no manual
decoding is necessary.

=item number

A JSON number becomes either an integer, numeric (floating point) or
string scalar in perl, depending on its range and any fractional parts. On
the Perl level, there is no difference between those as Perl handles all
the conversion details, but an integer may take slightly less memory and
might represent more values exactly than floating point numbers.

If the number consists of digits only, C<JSON> will try to represent
it as an integer value. If that fails, it will try to represent it as
a numeric (floating point) value if that is possible without loss of
precision. Otherwise it will preserve the number as a string value (in
which case you lose roundtripping ability, as the JSON number will be
re-encoded toa JSON string).

Numbers containing a fractional or exponential part will always be
represented as numeric (floating point) values, possibly at a loss of
precision (in which case you might lose perfect roundtripping ability, but
the JSON number will still be re-encoded as a JSON number).

Note that precision is not accuracy - binary floating point values cannot
represent most decimal fractions exactly, and when converting from and to
floating point, C<JSON> only guarantees precision up to but not including
the leats significant bit.

If the backend is JSON::PP and C<allow_bignum> is enable, the big integers 
and the numeric can be optionally converted into L<Math::BigInt> and
L<Math::BigFloat> objects.

=item true, false

These JSON atoms become C<JSON::true> and C<JSON::false>,
respectively. They are overloaded to act almost exactly like the numbers
C<1> and C<0>. You can check wether a scalar is a JSON boolean by using
the C<JSON::is_bool> function.

If C<JSON::true> and C<JSON::false> are used as strings or compared as strings,
they represent as C<true> and C<false> respectively.

   print JSON::true . "\n";
    => true
   print JSON::true + 1;
    => 1

   ok(JSON::true eq 'true');
   ok(JSON::true eq  '1');
   ok(JSON::true == 1);

C<JSON> will install these missing overloading features to the backend modules.


=item null

A JSON null atom becomes C<undef> in Perl.

C<JSON::null> returns C<unddef>.

=back


=head2 PERL -> JSON

The mapping from Perl to JSON is slightly more difficult, as Perl is a
truly typeless language, so we can only guess which JSON type is meant by
a Perl value.

=over 4

=item hash references

Perl hash references become JSON objects. As there is no inherent ordering
in hash keys (or JSON objects), they will usually be encoded in a
pseudo-random order that can change between runs of the same program but
stays generally the same within a single run of a program. C<JSON>
optionally sort the hash keys (determined by the I<canonical> flag), so
the same datastructure will serialise to the same JSON text (given same
settings and version of JSON::XS), but this incurs a runtime overhead
and is only rarely useful, e.g. when you want to compare some JSON text
against another for equality.

In future, the ordered object feature will be added to JSON::PP using C<tie> mechanism.


=item array references

Perl array references become JSON arrays.

=item other references

Other unblessed references are generally not allowed and will cause an
exception to be thrown, except for references to the integers C<0> and
C<1>, which get turned into C<false> and C<true> atoms in JSON. You can
also use C<JSON::false> and C<JSON::true> to improve readability.

   to_json [\0,JSON::true]      # yields [false,true]

=item JSON::true, JSON::false, JSON::null

These special values become JSON true and JSON false values,
respectively. You can also use C<\1> and C<\0> directly if you want.

JSON::null returns C<undef>.

=item blessed objects

Blessed objects are not directly representable in JSON. See the
C<allow_blessed> and C<convert_blessed> methods on various options on
how to deal with this: basically, you can choose between throwing an
exception, encoding the reference as if it weren't blessed, or provide
your own serialiser method.

With C<convert_blessed_universally> mode,  C<encode> converts blessed
hash references or blessed array references (contains other blessed references)
into JSON members and arrays.

   use JSON -convert_blessed_universally;
   JSON->new->allow_blessed->convert_blessed->encode( $blessed_object );

See to L<convert_blessed>.

=item simple scalars

Simple Perl scalars (any scalar that is not a reference) are the most
difficult objects to encode: JSON::XS and JSON::PP will encode undefined scalars as
JSON C<null> values, scalars that have last been used in a string context
before encoding as JSON strings, and anything else as number value:

   # dump as number
   encode_json [2]                      # yields [2]
   encode_json [-3.0e17]                # yields [-3e+17]
   my $value = 5; encode_json [$value]  # yields [5]

   # used as string, so dump as string
   print $value;
   encode_json [$value]                 # yields ["5"]

   # undef becomes null
   encode_json [undef]                  # yields [null]

You can force the type to be a string by stringifying it:

   my $x = 3.1; # some variable containing a number
   "$x";        # stringified
   $x .= "";    # another, more awkward way to stringify
   print $x;    # perl does it for you, too, quite often

You can force the type to be a number by numifying it:

   my $x = "3"; # some variable containing a string
   $x += 0;     # numify it, ensuring it will be dumped as a number
   $x *= 1;     # same thing, the choise is yours.

You can not currently force the type in other, less obscure, ways.

Note that numerical precision has the same meaning as under Perl (so
binary to decimal conversion follows the same rules as in Perl, which
can differ to other languages). Also, your perl interpreter might expose
extensions to the floating point numbers of your platform, such as
infinities or NaN's - these cannot be represented in JSON, and it is an
error to pass those in.

=item Big Number

If the backend is JSON::PP and C<allow_bignum> is enable, 
C<encode> converts C<Math::BigInt> objects and C<Math::BigFloat>
objects into JSON numbers.


=back

=head1 JSON and ECMAscript

See to L<JSON::XS/JSON and ECMAscript>.

=head1 JSON and YAML

JSON is not a subset of YAML.
See to L<JSON::XS/JSON and YAML>.


=head1 BACKEND MODULE DECISION

When you use C<JSON>, C<JSON> tries to C<use> JSON::XS. If this call failed, it will
C<uses> JSON::PP. The required JSON::XS version is I<2.2> or later.

The C<JSON> constructor method returns an object inherited from the backend module,
and JSON::XS object is a blessed scaler reference while JSON::PP is a blessed hash
reference.

So, your program should not depend on the backend module, especially
returned objects should not be modified.

 my $json = JSON->new; # XS or PP?
 $json->{stash} = 'this is xs object'; # this code may raise an error!

To check the backend module, there are some methods - C<backend>, C<is_pp> and C<is_xs>.

  JSON->backend; # 'JSON::XS' or 'JSON::PP'
  
  JSON->backend->is_pp: # 0 or 1
  
  JSON->backend->is_xs: # 1 or 0
  
  $json->is_xs; # 1 or 0
  
  $json->is_pp; # 0 or 1


If you set an enviornment variable C<PERL_JSON_BACKEND>, The calling action will be changed.

=over

=item PERL_JSON_BACKEND = 0 or PERL_JSON_BACKEND = 'JSON::PP'

Always use JSON::PP

=item PERL_JSON_BACKEND == 1 or PERL_JSON_BACKEND = 'JSON::XS,JSON::PP'

(The default) Use compiled JSON::XS if it is properly compiled & installed,
otherwise use JSON::PP.

=item PERL_JSON_BACKEND == 2 or PERL_JSON_BACKEND = 'JSON::XS'

Always use compiled JSON::XS, die if it isn't properly compiled & installed.

=item PERL_JSON_BACKEND = 'JSON::backportPP'

Always use JSON::backportPP.
JSON::backportPP is JSON::PP back port module.
C<JSON> includs JSON::backportPP instead of JSON::PP.

=back

These ideas come from L<DBI::PurePerl> mechanism.

example:

 BEGIN { $ENV{PERL_JSON_BACKEND} = 'JSON::PP' }
 use JSON; # always uses JSON::PP

In future, it may be able to specify another module.

=head1 USE PP FEATURES EVEN THOUGH XS BACKEND

Many methods are available with either JSON::XS or JSON::PP and
when the backend module is JSON::XS, if any JSON::PP specific (i.e. JSON::XS unspported)
method is called, it will C<warn> and be noop.

But If you C<use> C<JSON> passing the optional string C<-support_by_pp>,
it makes a part of those unupported methods available.
This feature is achieved by using JSON::PP in C<de/encode>.

   BEGIN { $ENV{PERL_JSON_BACKEND} = 2 } # with JSON::XS
   use JSON -support_by_pp;
   my $json = new JSON;
   $json->allow_nonref->escape_slash->encode("/");

At this time, the returned object is a C<JSON::Backend::XS::Supportable>
object (re-blessed XS object), and  by checking JSON::XS unsupported flags
in de/encoding, can support some unsupported methods - C<loose>, C<allow_bignum>,
C<allow_barekey>, C<allow_singlequote>, C<escape_slash> and C<indent_length>.

When any unsupported methods are not enable, C<XS de/encode> will be
used as is. The switch is achieved by changing the symbolic tables.

C<-support_by_pp> is effective only when the backend module is JSON::XS
and it makes the de/encoding speed down a bit.

See to L<JSON::PP SUPPORT METHODS>.

=head1 INCOMPATIBLE CHANGES TO OLD VERSION

There are big incompatibility between new version (2.00) and old (1.xx).
If you use old C<JSON> 1.xx in your code, please check it.

See to L<Transition ways from 1.xx to 2.xx.>

=over

=item jsonToObj and objToJson are obsoleted.

Non Perl-style name C<jsonToObj> and C<objToJson> are obsoleted
(but not yet deleted from the source).
If you use these functions in your code, please replace them
with C<from_json> and C<to_json>.


=item Global variables are no longer available.

C<JSON> class variables - C<$JSON::AUTOCONVERT>, C<$JSON::BareKey>, etc...
- are not available any longer.
Instead, various features can be used through object methods.


=item Package JSON::Converter and JSON::Parser are deleted.

Now C<JSON> bundles with JSON::PP which can handle JSON more properly than them.

=item Package JSON::NotString is deleted.

There was C<JSON::NotString> class which represents JSON value C<true>, C<false>, C<null>
and numbers. It was deleted and replaced by C<JSON::Boolean>.

C<JSON::Boolean> represents C<true> and C<false>.

C<JSON::Boolean> does not represent C<null>.

C<JSON::null> returns C<undef>.

C<JSON> makes L<JSON::XS::Boolean> and L<JSON::PP::Boolean> is-a relation
to L<JSON::Boolean>.

=item function JSON::Number is obsoleted.

C<JSON::Number> is now needless because JSON::XS and JSON::PP have
round-trip integrity.

=item JSONRPC modules are deleted.

Perl implementation of JSON-RPC protocol - C<JSONRPC >, C<JSONRPC::Transport::HTTP>
and C<Apache::JSONRPC > are deleted in this distribution.
Instead of them, there is L<JSON::RPC> which supports JSON-RPC protocol version 1.1.

=back

=head2 Transition ways from 1.xx to 2.xx.

You should set C<suport_by_pp> mode firstly, because
it is always successful for the below codes even with JSON::XS.

    use JSON -support_by_pp;

=over

=item Exported jsonToObj (simple)

  from_json($json_text);

=item Exported objToJson (simple)

  to_json($perl_scalar);

=item Exported jsonToObj (advanced)

  $flags = {allow_barekey => 1, allow_singlequote => 1};
  from_json($json_text, $flags);

equivalent to:

  $JSON::BareKey = 1;
  $JSON::QuotApos = 1;
  jsonToObj($json_text);

=item Exported objToJson (advanced)

  $flags = {allow_blessed => 1, allow_barekey => 1};
  to_json($perl_scalar, $flags);

equivalent to:

  $JSON::BareKey = 1;
  objToJson($perl_scalar);

=item jsonToObj as object method

  $json->decode($json_text);

=item objToJson as object method

  $json->encode($perl_scalar);

=item new method with parameters

The C<new> method in 2.x takes any parameters no longer.
You can set parameters instead;

   $json = JSON->new->pretty;

=item $JSON::Pretty, $JSON::Indent, $JSON::Delimiter

If C<indent> is enable, that means C<$JSON::Pretty> flag set. And
C<$JSON::Delimiter> was substituted by C<space_before> and C<space_after>.
In conclusion:

   $json->indent->space_before->space_after;

Equivalent to:

  $json->pretty;

To change indent length, use C<indent_length>.

(Only with JSON::PP, if C<-support_by_pp> is not used.)

  $json->pretty->indent_length(2)->encode($perl_scalar);

=item $JSON::BareKey

(Only with JSON::PP, if C<-support_by_pp> is not used.)

  $json->allow_barekey->decode($json_text)

=item $JSON::ConvBlessed

use C<-convert_blessed_universally>. See to L<convert_blessed>.

=item $JSON::QuotApos

(Only with JSON::PP, if C<-support_by_pp> is not used.)

  $json->allow_singlequote->decode($json_text)

=item $JSON::SingleQuote

Disable. C<JSON> does not make such a invalid JSON string any longer.

=item $JSON::KeySort

  $json->canonical->encode($perl_scalar)

This is the ascii sort.

If you want to use with your own sort routine, check the C<sort_by> method.

(Only with JSON::PP, even if C<-support_by_pp> is used currently.)

  $json->sort_by($sort_routine_ref)->encode($perl_scalar)
 
  $json->sort_by(sub { $JSON::PP::a <=> $JSON::PP::b })->encode($perl_scalar)

Can't access C<$a> and C<$b> but C<$JSON::PP::a> and C<$JSON::PP::b>.

=item $JSON::SkipInvalid

  $json->allow_unknown

=item $JSON::AUTOCONVERT

Needless. C<JSON> backend modules have the round-trip integrity.

=item $JSON::UTF8

Needless because C<JSON> (JSON::XS/JSON::PP) sets
the UTF8 flag on properly.

    # With UTF8-flagged strings

    $json->allow_nonref;
    $str = chr(1000); # UTF8-flagged

    $json_text  = $json->utf8(0)->encode($str);
    utf8::is_utf8($json_text);
    # true
    $json_text  = $json->utf8(1)->encode($str);
    utf8::is_utf8($json_text);
    # false

    $str = '"' . chr(1000) . '"'; # UTF8-flagged

    $perl_scalar  = $json->utf8(0)->decode($str);
    utf8::is_utf8($perl_scalar);
    # true
    $perl_scalar  = $json->utf8(1)->decode($str);
    # died because of 'Wide character in subroutine'

See to L<JSON::XS/A FEW NOTES ON UNICODE AND PERL>.

=item $JSON::UnMapping

Disable. See to L<MAPPING>.

=item $JSON::SelfConvert

This option was deleted.
Instead of it, if a givien blessed object has the C<TO_JSON> method,
C<TO_JSON> will be executed with C<convert_blessed>.

  $json->convert_blessed->encode($bleesed_hashref_or_arrayref)
  # if need, call allow_blessed

Note that it was C<toJson> in old version, but now not C<toJson> but C<TO_JSON>.

=back

=head1 TODO

=over

=item example programs

=back

=head1 THREADS

No test with JSON::PP. If with JSON::XS, See to L<JSON::XS/THREADS>.


=head1 BUGS

Please report bugs relevant to C<JSON> to E<lt>makamaka[at]cpan.orgE<gt>.


=head1 SEE ALSO

Most of the document is copied and modified from JSON::XS doc.

L<JSON::XS>, L<JSON::PP>

C<RFC4627>(L<http://www.ietf.org/rfc/rfc4627.txt>)

=head1 AUTHOR

Makamaka Hannyaharamitu, E<lt>makamaka[at]cpan.orgE<gt>

JSON::XS was written by  Marc Lehmann <schmorp[at]schmorp.de>

The relese of this new version owes to the courtesy of Marc Lehmann.


=head1 COPYRIGHT AND LICENSE

Copyright 2005-2011 by Makamaka Hannyaharamitu

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut

