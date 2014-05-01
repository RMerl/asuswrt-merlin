package # This is JSON::backportPP
    JSON::PP;

# JSON-2.0

use 5.005;
use strict;
use base qw(Exporter);
use overload ();

use Carp ();
use B ();
#use Devel::Peek;

$JSON::PP::VERSION = '2.27200';

@JSON::PP::EXPORT = qw(encode_json decode_json from_json to_json);

# instead of hash-access, i tried index-access for speed.
# but this method is not faster than what i expected. so it will be changed.

use constant P_ASCII                => 0;
use constant P_LATIN1               => 1;
use constant P_UTF8                 => 2;
use constant P_INDENT               => 3;
use constant P_CANONICAL            => 4;
use constant P_SPACE_BEFORE         => 5;
use constant P_SPACE_AFTER          => 6;
use constant P_ALLOW_NONREF         => 7;
use constant P_SHRINK               => 8;
use constant P_ALLOW_BLESSED        => 9;
use constant P_CONVERT_BLESSED      => 10;
use constant P_RELAXED              => 11;

use constant P_LOOSE                => 12;
use constant P_ALLOW_BIGNUM         => 13;
use constant P_ALLOW_BAREKEY        => 14;
use constant P_ALLOW_SINGLEQUOTE    => 15;
use constant P_ESCAPE_SLASH         => 16;
use constant P_AS_NONBLESSED        => 17;

use constant P_ALLOW_UNKNOWN        => 18;

use constant OLD_PERL => $] < 5.008 ? 1 : 0;

BEGIN {
    my @xs_compati_bit_properties = qw(
            latin1 ascii utf8 indent canonical space_before space_after allow_nonref shrink
            allow_blessed convert_blessed relaxed allow_unknown
    );
    my @pp_bit_properties = qw(
            allow_singlequote allow_bignum loose
            allow_barekey escape_slash as_nonblessed
    );

    # Perl version check, Unicode handling is enable?
    # Helper module sets @JSON::PP::_properties.
    if ($] < 5.008 ) {
        my $helper = $] >= 5.006 ? 'JSON::backportPP::Compat5006' : 'JSON::backportPP::Compat5005';
        eval qq| require $helper |;
        if ($@) { Carp::croak $@; }
    }

    for my $name (@xs_compati_bit_properties, @pp_bit_properties) {
        my $flag_name = 'P_' . uc($name);

        eval qq/
            sub $name {
                my \$enable = defined \$_[1] ? \$_[1] : 1;

                if (\$enable) {
                    \$_[0]->{PROPS}->[$flag_name] = 1;
                }
                else {
                    \$_[0]->{PROPS}->[$flag_name] = 0;
                }

                \$_[0];
            }

            sub get_$name {
                \$_[0]->{PROPS}->[$flag_name] ? 1 : '';
            }
        /;
    }

}



# Functions

my %encode_allow_method
     = map {($_ => 1)} qw/utf8 pretty allow_nonref latin1 self_encode escape_slash
                          allow_blessed convert_blessed indent indent_length allow_bignum
                          as_nonblessed
                        /;
my %decode_allow_method
     = map {($_ => 1)} qw/utf8 allow_nonref loose allow_singlequote allow_bignum
                          allow_barekey max_size relaxed/;


my $JSON; # cache

sub encode_json ($) { # encode
    ($JSON ||= __PACKAGE__->new->utf8)->encode(@_);
}


sub decode_json { # decode
    ($JSON ||= __PACKAGE__->new->utf8)->decode(@_);
}

# Obsoleted

sub to_json($) {
   Carp::croak ("JSON::PP::to_json has been renamed to encode_json.");
}


sub from_json($) {
   Carp::croak ("JSON::PP::from_json has been renamed to decode_json.");
}


# Methods

sub new {
    my $class = shift;
    my $self  = {
        max_depth   => 512,
        max_size    => 0,
        indent      => 0,
        FLAGS       => 0,
        fallback      => sub { encode_error('Invalid value. JSON can only reference.') },
        indent_length => 3,
    };

    bless $self, $class;
}


sub encode {
    return $_[0]->PP_encode_json($_[1]);
}


sub decode {
    return $_[0]->PP_decode_json($_[1], 0x00000000);
}


sub decode_prefix {
    return $_[0]->PP_decode_json($_[1], 0x00000001);
}


# accessor


# pretty printing

sub pretty {
    my ($self, $v) = @_;
    my $enable = defined $v ? $v : 1;

    if ($enable) { # indent_length(3) for JSON::XS compatibility
        $self->indent(1)->indent_length(3)->space_before(1)->space_after(1);
    }
    else {
        $self->indent(0)->space_before(0)->space_after(0);
    }

    $self;
}

# etc

sub max_depth {
    my $max  = defined $_[1] ? $_[1] : 0x80000000;
    $_[0]->{max_depth} = $max;
    $_[0];
}


sub get_max_depth { $_[0]->{max_depth}; }


sub max_size {
    my $max  = defined $_[1] ? $_[1] : 0;
    $_[0]->{max_size} = $max;
    $_[0];
}


sub get_max_size { $_[0]->{max_size}; }


sub filter_json_object {
    $_[0]->{cb_object} = defined $_[1] ? $_[1] : 0;
    $_[0]->{F_HOOK} = ($_[0]->{cb_object} or $_[0]->{cb_sk_object}) ? 1 : 0;
    $_[0];
}

sub filter_json_single_key_object {
    if (@_ > 1) {
        $_[0]->{cb_sk_object}->{$_[1]} = $_[2];
    }
    $_[0]->{F_HOOK} = ($_[0]->{cb_object} or $_[0]->{cb_sk_object}) ? 1 : 0;
    $_[0];
}

sub indent_length {
    if (!defined $_[1] or $_[1] > 15 or $_[1] < 0) {
        Carp::carp "The acceptable range of indent_length() is 0 to 15.";
    }
    else {
        $_[0]->{indent_length} = $_[1];
    }
    $_[0];
}

sub get_indent_length {
    $_[0]->{indent_length};
}

sub sort_by {
    $_[0]->{sort_by} = defined $_[1] ? $_[1] : 1;
    $_[0];
}

sub allow_bigint {
    Carp::carp("allow_bigint() is obsoleted. use allow_bignum() insted.");
}

###############################

###
### Perl => JSON
###


{ # Convert

    my $max_depth;
    my $indent;
    my $ascii;
    my $latin1;
    my $utf8;
    my $space_before;
    my $space_after;
    my $canonical;
    my $allow_blessed;
    my $convert_blessed;

    my $indent_length;
    my $escape_slash;
    my $bignum;
    my $as_nonblessed;

    my $depth;
    my $indent_count;
    my $keysort;


    sub PP_encode_json {
        my $self = shift;
        my $obj  = shift;

        $indent_count = 0;
        $depth        = 0;

        my $idx = $self->{PROPS};

        ($ascii, $latin1, $utf8, $indent, $canonical, $space_before, $space_after, $allow_blessed,
            $convert_blessed, $escape_slash, $bignum, $as_nonblessed)
         = @{$idx}[P_ASCII .. P_SPACE_AFTER, P_ALLOW_BLESSED, P_CONVERT_BLESSED,
                    P_ESCAPE_SLASH, P_ALLOW_BIGNUM, P_AS_NONBLESSED];

        ($max_depth, $indent_length) = @{$self}{qw/max_depth indent_length/};

        $keysort = $canonical ? sub { $a cmp $b } : undef;

        if ($self->{sort_by}) {
            $keysort = ref($self->{sort_by}) eq 'CODE' ? $self->{sort_by}
                     : $self->{sort_by} =~ /\D+/       ? $self->{sort_by}
                     : sub { $a cmp $b };
        }

        encode_error("hash- or arrayref expected (not a simple scalar, use allow_nonref to allow this)")
             if(!ref $obj and !$idx->[ P_ALLOW_NONREF ]);

        my $str  = $self->object_to_json($obj);

        $str .= "\n" if ( $indent ); # JSON::XS 2.26 compatible

        unless ($ascii or $latin1 or $utf8) {
            utf8::upgrade($str);
        }

        if ($idx->[ P_SHRINK ]) {
            utf8::downgrade($str, 1);
        }

        return $str;
    }


    sub object_to_json {
        my ($self, $obj) = @_;
        my $type = ref($obj);

        if($type eq 'HASH'){
            return $self->hash_to_json($obj);
        }
        elsif($type eq 'ARRAY'){
            return $self->array_to_json($obj);
        }
        elsif ($type) { # blessed object?
            if (blessed($obj)) {

                return $self->value_to_json($obj) if ( $obj->isa('JSON::PP::Boolean') );

                if ( $convert_blessed and $obj->can('TO_JSON') ) {
                    my $result = $obj->TO_JSON();
                    if ( defined $result and ref( $result ) ) {
                        if ( refaddr( $obj ) eq refaddr( $result ) ) {
                            encode_error( sprintf(
                                "%s::TO_JSON method returned same object as was passed instead of a new one",
                                ref $obj
                            ) );
                        }
                    }

                    return $self->object_to_json( $result );
                }

                return "$obj" if ( $bignum and _is_bignum($obj) );
                return $self->blessed_to_json($obj) if ($allow_blessed and $as_nonblessed); # will be removed.

                encode_error( sprintf("encountered object '%s', but neither allow_blessed "
                    . "nor convert_blessed settings are enabled", $obj)
                ) unless ($allow_blessed);

                return 'null';
            }
            else {
                return $self->value_to_json($obj);
            }
        }
        else{
            return $self->value_to_json($obj);
        }
    }


    sub hash_to_json {
        my ($self, $obj) = @_;
        my @res;

        encode_error("json text or perl structure exceeds maximum nesting level (max_depth set too low?)")
                                         if (++$depth > $max_depth);

        my ($pre, $post) = $indent ? $self->_up_indent() : ('', '');
        my $del = ($space_before ? ' ' : '') . ':' . ($space_after ? ' ' : '');

        for my $k ( _sort( $obj ) ) {
            if ( OLD_PERL ) { utf8::decode($k) } # key for Perl 5.6 / be optimized
            push @res, string_to_json( $self, $k )
                          .  $del
                          . ( $self->object_to_json( $obj->{$k} ) || $self->value_to_json( $obj->{$k} ) );
        }

        --$depth;
        $self->_down_indent() if ($indent);

        return   '{' . ( @res ? $pre : '' ) . ( @res ? join( ",$pre", @res ) . $post : '' )  . '}';
    }


    sub array_to_json {
        my ($self, $obj) = @_;
        my @res;

        encode_error("json text or perl structure exceeds maximum nesting level (max_depth set too low?)")
                                         if (++$depth > $max_depth);

        my ($pre, $post) = $indent ? $self->_up_indent() : ('', '');

        for my $v (@$obj){
            push @res, $self->object_to_json($v) || $self->value_to_json($v);
        }

        --$depth;
        $self->_down_indent() if ($indent);

        return '[' . ( @res ? $pre : '' ) . ( @res ? join( ",$pre", @res ) . $post : '' ) . ']';
    }


    sub value_to_json {
        my ($self, $value) = @_;

        return 'null' if(!defined $value);

        my $b_obj = B::svref_2object(\$value);  # for round trip problem
        my $flags = $b_obj->FLAGS;

        return $value # as is 
            if $flags & ( B::SVp_IOK | B::SVp_NOK ) and !( $flags & B::SVp_POK ); # SvTYPE is IV or NV?

        my $type = ref($value);

        if(!$type){
            return string_to_json($self, $value);
        }
        elsif( blessed($value) and  $value->isa('JSON::PP::Boolean') ){
            return $$value == 1 ? 'true' : 'false';
        }
        elsif ($type) {
            if ((overload::StrVal($value) =~ /=(\w+)/)[0]) {
                return $self->value_to_json("$value");
            }

            if ($type eq 'SCALAR' and defined $$value) {
                return   $$value eq '1' ? 'true'
                       : $$value eq '0' ? 'false'
                       : $self->{PROPS}->[ P_ALLOW_UNKNOWN ] ? 'null'
                       : encode_error("cannot encode reference to scalar");
            }

             if ( $self->{PROPS}->[ P_ALLOW_UNKNOWN ] ) {
                 return 'null';
             }
             else {
                 if ( $type eq 'SCALAR' or $type eq 'REF' ) {
                    encode_error("cannot encode reference to scalar");
                 }
                 else {
                    encode_error("encountered $value, but JSON can only represent references to arrays or hashes");
                 }
             }

        }
        else {
            return $self->{fallback}->($value)
                 if ($self->{fallback} and ref($self->{fallback}) eq 'CODE');
            return 'null';
        }

    }


    my %esc = (
        "\n" => '\n',
        "\r" => '\r',
        "\t" => '\t',
        "\f" => '\f',
        "\b" => '\b',
        "\"" => '\"',
        "\\" => '\\\\',
        "\'" => '\\\'',
    );


    sub string_to_json {
        my ($self, $arg) = @_;

        $arg =~ s/([\x22\x5c\n\r\t\f\b])/$esc{$1}/g;
        $arg =~ s/\//\\\//g if ($escape_slash);
        $arg =~ s/([\x00-\x08\x0b\x0e-\x1f])/'\\u00' . unpack('H2', $1)/eg;

        if ($ascii) {
            $arg = JSON_PP_encode_ascii($arg);
        }

        if ($latin1) {
            $arg = JSON_PP_encode_latin1($arg);
        }

        if ($utf8) {
            utf8::encode($arg);
        }

        return '"' . $arg . '"';
    }


    sub blessed_to_json {
        my $reftype = reftype($_[1]) || '';
        if ($reftype eq 'HASH') {
            return $_[0]->hash_to_json($_[1]);
        }
        elsif ($reftype eq 'ARRAY') {
            return $_[0]->array_to_json($_[1]);
        }
        else {
            return 'null';
        }
    }


    sub encode_error {
        my $error  = shift;
        Carp::croak "$error";
    }


    sub _sort {
        defined $keysort ? (sort $keysort (keys %{$_[0]})) : keys %{$_[0]};
    }


    sub _up_indent {
        my $self  = shift;
        my $space = ' ' x $indent_length;

        my ($pre,$post) = ('','');

        $post = "\n" . $space x $indent_count;

        $indent_count++;

        $pre = "\n" . $space x $indent_count;

        return ($pre,$post);
    }


    sub _down_indent { $indent_count--; }


    sub PP_encode_box {
        {
            depth        => $depth,
            indent_count => $indent_count,
        };
    }

} # Convert


sub _encode_ascii {
    join('',
        map {
            $_ <= 127 ?
                chr($_) :
            $_ <= 65535 ?
                sprintf('\u%04x', $_) : sprintf('\u%x\u%x', _encode_surrogates($_));
        } unpack('U*', $_[0])
    );
}


sub _encode_latin1 {
    join('',
        map {
            $_ <= 255 ?
                chr($_) :
            $_ <= 65535 ?
                sprintf('\u%04x', $_) : sprintf('\u%x\u%x', _encode_surrogates($_));
        } unpack('U*', $_[0])
    );
}


sub _encode_surrogates { # from perlunicode
    my $uni = $_[0] - 0x10000;
    return ($uni / 0x400 + 0xD800, $uni % 0x400 + 0xDC00);
}


sub _is_bignum {
    $_[0]->isa('Math::BigInt') or $_[0]->isa('Math::BigFloat');
}



#
# JSON => Perl
#

my $max_intsize;

BEGIN {
    my $checkint = 1111;
    for my $d (5..64) {
        $checkint .= 1;
        my $int   = eval qq| $checkint |;
        if ($int =~ /[eE]/) {
            $max_intsize = $d - 1;
            last;
        }
    }
}

{ # PARSE 

    my %escapes = ( #  by Jeremy Muhlich <jmuhlich [at] bitflood.org>
        b    => "\x8",
        t    => "\x9",
        n    => "\xA",
        f    => "\xC",
        r    => "\xD",
        '\\' => '\\',
        '"'  => '"',
        '/'  => '/',
    );

    my $text; # json data
    my $at;   # offset
    my $ch;   # 1chracter
    my $len;  # text length (changed according to UTF8 or NON UTF8)
    # INTERNAL
    my $depth;          # nest counter
    my $encoding;       # json text encoding
    my $is_valid_utf8;  # temp variable
    my $utf8_len;       # utf8 byte length
    # FLAGS
    my $utf8;           # must be utf8
    my $max_depth;      # max nest nubmer of objects and arrays
    my $max_size;
    my $relaxed;
    my $cb_object;
    my $cb_sk_object;

    my $F_HOOK;

    my $allow_bigint;   # using Math::BigInt
    my $singlequote;    # loosely quoting
    my $loose;          # 
    my $allow_barekey;  # bareKey

    # $opt flag
    # 0x00000001 .... decode_prefix
    # 0x10000000 .... incr_parse

    sub PP_decode_json {
        my ($self, $opt); # $opt is an effective flag during this decode_json.

        ($self, $text, $opt) = @_;

        ($at, $ch, $depth) = (0, '', 0);

        if ( !defined $text or ref $text ) {
            decode_error("malformed JSON string, neither array, object, number, string or atom");
        }

        my $idx = $self->{PROPS};

        ($utf8, $relaxed, $loose, $allow_bigint, $allow_barekey, $singlequote)
            = @{$idx}[P_UTF8, P_RELAXED, P_LOOSE .. P_ALLOW_SINGLEQUOTE];

        if ( $utf8 ) {
            utf8::downgrade( $text, 1 ) or Carp::croak("Wide character in subroutine entry");
        }
        else {
            utf8::upgrade( $text );
        }

        $len = length $text;

        ($max_depth, $max_size, $cb_object, $cb_sk_object, $F_HOOK)
             = @{$self}{qw/max_depth  max_size cb_object cb_sk_object F_HOOK/};

        if ($max_size > 1) {
            use bytes;
            my $bytes = length $text;
            decode_error(
                sprintf("attempted decode of JSON text of %s bytes size, but max_size is set to %s"
                    , $bytes, $max_size), 1
            ) if ($bytes > $max_size);
        }

        # Currently no effect
        # should use regexp
        my @octets = unpack('C4', $text);
        $encoding =   ( $octets[0] and  $octets[1]) ? 'UTF-8'
                    : (!$octets[0] and  $octets[1]) ? 'UTF-16BE'
                    : (!$octets[0] and !$octets[1]) ? 'UTF-32BE'
                    : ( $octets[2]                ) ? 'UTF-16LE'
                    : (!$octets[2]                ) ? 'UTF-32LE'
                    : 'unknown';

        white(); # remove head white space

        my $valid_start = defined $ch; # Is there a first character for JSON structure?

        my $result = value();

        return undef if ( !$result && ( $opt & 0x10000000 ) ); # for incr_parse

        decode_error("malformed JSON string, neither array, object, number, string or atom") unless $valid_start;

        if ( !$idx->[ P_ALLOW_NONREF ] and !ref $result ) {
                decode_error(
                'JSON text must be an object or array (but found number, string, true, false or null,'
                       . ' use allow_nonref to allow this)', 1);
        }

        Carp::croak('something wrong.') if $len < $at; # we won't arrive here.

        my $consumed = defined $ch ? $at - 1 : $at; # consumed JSON text length

        white(); # remove tail white space

        if ( $ch ) {
            return ( $result, $consumed ) if ($opt & 0x00000001); # all right if decode_prefix
            decode_error("garbage after JSON object");
        }

        ( $opt & 0x00000001 ) ? ( $result, $consumed ) : $result;
    }


    sub next_chr {
        return $ch = undef if($at >= $len);
        $ch = substr($text, $at++, 1);
    }


    sub value {
        white();
        return          if(!defined $ch);
        return object() if($ch eq '{');
        return array()  if($ch eq '[');
        return string() if($ch eq '"' or ($singlequote and $ch eq "'"));
        return number() if($ch =~ /[0-9]/ or $ch eq '-');
        return word();
    }

    sub string {
        my ($i, $s, $t, $u);
        my $utf16;
        my $is_utf8;

        ($is_valid_utf8, $utf8_len) = ('', 0);

        $s = ''; # basically UTF8 flag on

        if($ch eq '"' or ($singlequote and $ch eq "'")){
            my $boundChar = $ch;

            OUTER: while( defined(next_chr()) ){

                if($ch eq $boundChar){
                    next_chr();

                    if ($utf16) {
                        decode_error("missing low surrogate character in surrogate pair");
                    }

                    utf8::decode($s) if($is_utf8);

                    return $s;
                }
                elsif($ch eq '\\'){
                    next_chr();
                    if(exists $escapes{$ch}){
                        $s .= $escapes{$ch};
                    }
                    elsif($ch eq 'u'){ # UNICODE handling
                        my $u = '';

                        for(1..4){
                            $ch = next_chr();
                            last OUTER if($ch !~ /[0-9a-fA-F]/);
                            $u .= $ch;
                        }

                        # U+D800 - U+DBFF
                        if ($u =~ /^[dD][89abAB][0-9a-fA-F]{2}/) { # UTF-16 high surrogate?
                            $utf16 = $u;
                        }
                        # U+DC00 - U+DFFF
                        elsif ($u =~ /^[dD][c-fC-F][0-9a-fA-F]{2}/) { # UTF-16 low surrogate?
                            unless (defined $utf16) {
                                decode_error("missing high surrogate character in surrogate pair");
                            }
                            $is_utf8 = 1;
                            $s .= JSON_PP_decode_surrogates($utf16, $u) || next;
                            $utf16 = undef;
                        }
                        else {
                            if (defined $utf16) {
                                decode_error("surrogate pair expected");
                            }

                            if ( ( my $hex = hex( $u ) ) > 127 ) {
                                $is_utf8 = 1;
                                $s .= JSON_PP_decode_unicode($u) || next;
                            }
                            else {
                                $s .= chr $hex;
                            }
                        }

                    }
                    else{
                        unless ($loose) {
                            $at -= 2;
                            decode_error('illegal backslash escape sequence in string');
                        }
                        $s .= $ch;
                    }
                }
                else{

                    if ( ord $ch  > 127 ) {
                        if ( $utf8 ) {
                            unless( $ch = is_valid_utf8($ch) ) {
                                $at -= 1;
                                decode_error("malformed UTF-8 character in JSON string");
                            }
                            else {
                                $at += $utf8_len - 1;
                            }
                        }
                        else {
                            utf8::encode( $ch );
                        }

                        $is_utf8 = 1;
                    }

                    if (!$loose) {
                        if ($ch =~ /[\x00-\x1f\x22\x5c]/)  { # '/' ok
                            $at--;
                            decode_error('invalid character encountered while parsing JSON string');
                        }
                    }

                    $s .= $ch;
                }
            }
        }

        decode_error("unexpected end of string while parsing JSON string");
    }


    sub white {
        while( defined $ch  ){
            if($ch le ' '){
                next_chr();
            }
            elsif($ch eq '/'){
                next_chr();
                if(defined $ch and $ch eq '/'){
                    1 while(defined(next_chr()) and $ch ne "\n" and $ch ne "\r");
                }
                elsif(defined $ch and $ch eq '*'){
                    next_chr();
                    while(1){
                        if(defined $ch){
                            if($ch eq '*'){
                                if(defined(next_chr()) and $ch eq '/'){
                                    next_chr();
                                    last;
                                }
                            }
                            else{
                                next_chr();
                            }
                        }
                        else{
                            decode_error("Unterminated comment");
                        }
                    }
                    next;
                }
                else{
                    $at--;
                    decode_error("malformed JSON string, neither array, object, number, string or atom");
                }
            }
            else{
                if ($relaxed and $ch eq '#') { # correctly?
                    pos($text) = $at;
                    $text =~ /\G([^\n]*(?:\r\n|\r|\n|$))/g;
                    $at = pos($text);
                    next_chr;
                    next;
                }

                last;
            }
        }
    }


    sub array {
        my $a  = $_[0] || []; # you can use this code to use another array ref object.

        decode_error('json text or perl structure exceeds maximum nesting level (max_depth set too low?)')
                                                    if (++$depth > $max_depth);

        next_chr();
        white();

        if(defined $ch and $ch eq ']'){
            --$depth;
            next_chr();
            return $a;
        }
        else {
            while(defined($ch)){
                push @$a, value();

                white();

                if (!defined $ch) {
                    last;
                }

                if($ch eq ']'){
                    --$depth;
                    next_chr();
                    return $a;
                }

                if($ch ne ','){
                    last;
                }

                next_chr();
                white();

                if ($relaxed and $ch eq ']') {
                    --$depth;
                    next_chr();
                    return $a;
                }

            }
        }

        decode_error(", or ] expected while parsing array");
    }


    sub object {
        my $o = $_[0] || {}; # you can use this code to use another hash ref object.
        my $k;

        decode_error('json text or perl structure exceeds maximum nesting level (max_depth set too low?)')
                                                if (++$depth > $max_depth);
        next_chr();
        white();

        if(defined $ch and $ch eq '}'){
            --$depth;
            next_chr();
            if ($F_HOOK) {
                return _json_object_hook($o);
            }
            return $o;
        }
        else {
            while (defined $ch) {
                $k = ($allow_barekey and $ch ne '"' and $ch ne "'") ? bareKey() : string();
                white();

                if(!defined $ch or $ch ne ':'){
                    $at--;
                    decode_error("':' expected");
                }

                next_chr();
                $o->{$k} = value();
                white();

                last if (!defined $ch);

                if($ch eq '}'){
                    --$depth;
                    next_chr();
                    if ($F_HOOK) {
                        return _json_object_hook($o);
                    }
                    return $o;
                }

                if($ch ne ','){
                    last;
                }

                next_chr();
                white();

                if ($relaxed and $ch eq '}') {
                    --$depth;
                    next_chr();
                    if ($F_HOOK) {
                        return _json_object_hook($o);
                    }
                    return $o;
                }

            }

        }

        $at--;
        decode_error(", or } expected while parsing object/hash");
    }


    sub bareKey { # doesn't strictly follow Standard ECMA-262 3rd Edition
        my $key;
        while($ch =~ /[^\x00-\x23\x25-\x2F\x3A-\x40\x5B-\x5E\x60\x7B-\x7F]/){
            $key .= $ch;
            next_chr();
        }
        return $key;
    }


    sub word {
        my $word =  substr($text,$at-1,4);

        if($word eq 'true'){
            $at += 3;
            next_chr;
            return $JSON::PP::true;
        }
        elsif($word eq 'null'){
            $at += 3;
            next_chr;
            return undef;
        }
        elsif($word eq 'fals'){
            $at += 3;
            if(substr($text,$at,1) eq 'e'){
                $at++;
                next_chr;
                return $JSON::PP::false;
            }
        }

        $at--; # for decode_error report

        decode_error("'null' expected")  if ($word =~ /^n/);
        decode_error("'true' expected")  if ($word =~ /^t/);
        decode_error("'false' expected") if ($word =~ /^f/);
        decode_error("malformed JSON string, neither array, object, number, string or atom");
    }


    sub number {
        my $n    = '';
        my $v;

        # According to RFC4627, hex or oct digts are invalid.
        if($ch eq '0'){
            my $peek = substr($text,$at,1);
            my $hex  = $peek =~ /[xX]/; # 0 or 1

            if($hex){
                decode_error("malformed number (leading zero must not be followed by another digit)");
                ($n) = ( substr($text, $at+1) =~ /^([0-9a-fA-F]+)/);
            }
            else{ # oct
                ($n) = ( substr($text, $at) =~ /^([0-7]+)/);
                if (defined $n and length $n > 1) {
                    decode_error("malformed number (leading zero must not be followed by another digit)");
                }
            }

            if(defined $n and length($n)){
                if (!$hex and length($n) == 1) {
                   decode_error("malformed number (leading zero must not be followed by another digit)");
                }
                $at += length($n) + $hex;
                next_chr;
                return $hex ? hex($n) : oct($n);
            }
        }

        if($ch eq '-'){
            $n = '-';
            next_chr;
            if (!defined $ch or $ch !~ /\d/) {
                decode_error("malformed number (no digits after initial minus)");
            }
        }

        while(defined $ch and $ch =~ /\d/){
            $n .= $ch;
            next_chr;
        }

        if(defined $ch and $ch eq '.'){
            $n .= '.';

            next_chr;
            if (!defined $ch or $ch !~ /\d/) {
                decode_error("malformed number (no digits after decimal point)");
            }
            else {
                $n .= $ch;
            }

            while(defined(next_chr) and $ch =~ /\d/){
                $n .= $ch;
            }
        }

        if(defined $ch and ($ch eq 'e' or $ch eq 'E')){
            $n .= $ch;
            next_chr;

            if(defined($ch) and ($ch eq '+' or $ch eq '-')){
                $n .= $ch;
                next_chr;
                if (!defined $ch or $ch =~ /\D/) {
                    decode_error("malformed number (no digits after exp sign)");
                }
                $n .= $ch;
            }
            elsif(defined($ch) and $ch =~ /\d/){
                $n .= $ch;
            }
            else {
                decode_error("malformed number (no digits after exp sign)");
            }

            while(defined(next_chr) and $ch =~ /\d/){
                $n .= $ch;
            }

        }

        $v .= $n;

        if ($v !~ /[.eE]/ and length $v > $max_intsize) {
            if ($allow_bigint) { # from Adam Sussman
                require Math::BigInt;
                return Math::BigInt->new($v);
            }
            else {
                return "$v";
            }
        }
        elsif ($allow_bigint) {
            require Math::BigFloat;
            return Math::BigFloat->new($v);
        }

        return 0+$v;
    }


    sub is_valid_utf8 {

        $utf8_len = $_[0] =~ /[\x00-\x7F]/  ? 1
                  : $_[0] =~ /[\xC2-\xDF]/  ? 2
                  : $_[0] =~ /[\xE0-\xEF]/  ? 3
                  : $_[0] =~ /[\xF0-\xF4]/  ? 4
                  : 0
                  ;

        return unless $utf8_len;

        my $is_valid_utf8 = substr($text, $at - 1, $utf8_len);

        return ( $is_valid_utf8 =~ /^(?:
             [\x00-\x7F]
            |[\xC2-\xDF][\x80-\xBF]
            |[\xE0][\xA0-\xBF][\x80-\xBF]
            |[\xE1-\xEC][\x80-\xBF][\x80-\xBF]
            |[\xED][\x80-\x9F][\x80-\xBF]
            |[\xEE-\xEF][\x80-\xBF][\x80-\xBF]
            |[\xF0][\x90-\xBF][\x80-\xBF][\x80-\xBF]
            |[\xF1-\xF3][\x80-\xBF][\x80-\xBF][\x80-\xBF]
            |[\xF4][\x80-\x8F][\x80-\xBF][\x80-\xBF]
        )$/x )  ? $is_valid_utf8 : '';
    }


    sub decode_error {
        my $error  = shift;
        my $no_rep = shift;
        my $str    = defined $text ? substr($text, $at) : '';
        my $mess   = '';
        my $type   = $] >= 5.008           ? 'U*'
                   : $] <  5.006           ? 'C*'
                   : utf8::is_utf8( $str ) ? 'U*' # 5.6
                   : 'C*'
                   ;

        for my $c ( unpack( $type, $str ) ) { # emulate pv_uni_display() ?
            $mess .=  $c == 0x07 ? '\a'
                    : $c == 0x09 ? '\t'
                    : $c == 0x0a ? '\n'
                    : $c == 0x0d ? '\r'
                    : $c == 0x0c ? '\f'
                    : $c <  0x20 ? sprintf('\x{%x}', $c)
                    : $c == 0x5c ? '\\\\'
                    : $c <  0x80 ? chr($c)
                    : sprintf('\x{%x}', $c)
                    ;
            if ( length $mess >= 20 ) {
                $mess .= '...';
                last;
            }
        }

        unless ( length $mess ) {
            $mess = '(end of string)';
        }

        Carp::croak (
            $no_rep ? "$error" : "$error, at character offset $at (before \"$mess\")"
        );

    }


    sub _json_object_hook {
        my $o    = $_[0];
        my @ks = keys %{$o};

        if ( $cb_sk_object and @ks == 1 and exists $cb_sk_object->{ $ks[0] } and ref $cb_sk_object->{ $ks[0] } ) {
            my @val = $cb_sk_object->{ $ks[0] }->( $o->{$ks[0]} );
            if (@val == 1) {
                return $val[0];
            }
        }

        my @val = $cb_object->($o) if ($cb_object);
        if (@val == 0 or @val > 1) {
            return $o;
        }
        else {
            return $val[0];
        }
    }


    sub PP_decode_box {
        {
            text    => $text,
            at      => $at,
            ch      => $ch,
            len     => $len,
            depth   => $depth,
            encoding      => $encoding,
            is_valid_utf8 => $is_valid_utf8,
        };
    }

} # PARSE


sub _decode_surrogates { # from perlunicode
    my $uni = 0x10000 + (hex($_[0]) - 0xD800) * 0x400 + (hex($_[1]) - 0xDC00);
    my $un  = pack('U*', $uni);
    utf8::encode( $un );
    return $un;
}


sub _decode_unicode {
    my $un = pack('U', hex shift);
    utf8::encode( $un );
    return $un;
}

#
# Setup for various Perl versions (the code from JSON::PP58)
#

BEGIN {

    unless ( defined &utf8::is_utf8 ) {
       require Encode;
       *utf8::is_utf8 = *Encode::is_utf8;
    }

    if ( $] >= 5.008 ) {
        *JSON::PP::JSON_PP_encode_ascii      = \&_encode_ascii;
        *JSON::PP::JSON_PP_encode_latin1     = \&_encode_latin1;
        *JSON::PP::JSON_PP_decode_surrogates = \&_decode_surrogates;
        *JSON::PP::JSON_PP_decode_unicode    = \&_decode_unicode;
    }

    if ($] >= 5.008 and $] < 5.008003) { # join() in 5.8.0 - 5.8.2 is broken.
        package JSON::PP;
        require subs;
        subs->import('join');
        eval q|
            sub join {
                return '' if (@_ < 2);
                my $j   = shift;
                my $str = shift;
                for (@_) { $str .= $j . $_; }
                return $str;
            }
        |;
    }


    sub JSON::PP::incr_parse {
        local $Carp::CarpLevel = 1;
        ( $_[0]->{_incr_parser} ||= JSON::PP::IncrParser->new )->incr_parse( @_ );
    }


    sub JSON::PP::incr_skip {
        ( $_[0]->{_incr_parser} ||= JSON::PP::IncrParser->new )->incr_skip;
    }


    sub JSON::PP::incr_reset {
        ( $_[0]->{_incr_parser} ||= JSON::PP::IncrParser->new )->incr_reset;
    }

    eval q{
        sub JSON::PP::incr_text : lvalue {
            $_[0]->{_incr_parser} ||= JSON::PP::IncrParser->new;

            if ( $_[0]->{_incr_parser}->{incr_parsing} ) {
                Carp::croak("incr_text can not be called when the incremental parser already started parsing");
            }
            $_[0]->{_incr_parser}->{incr_text};
        }
    } if ( $] >= 5.006 );

} # Setup for various Perl versions (the code from JSON::PP58)


###############################
# Utilities
#

BEGIN {
    eval 'require Scalar::Util';
    unless($@){
        *JSON::PP::blessed = \&Scalar::Util::blessed;
        *JSON::PP::reftype = \&Scalar::Util::reftype;
        *JSON::PP::refaddr = \&Scalar::Util::refaddr;
    }
    else{ # This code is from Sclar::Util.
        # warn $@;
        eval 'sub UNIVERSAL::a_sub_not_likely_to_be_here { ref($_[0]) }';
        *JSON::PP::blessed = sub {
            local($@, $SIG{__DIE__}, $SIG{__WARN__});
            ref($_[0]) ? eval { $_[0]->a_sub_not_likely_to_be_here } : undef;
        };
        my %tmap = qw(
            B::NULL   SCALAR
            B::HV     HASH
            B::AV     ARRAY
            B::CV     CODE
            B::IO     IO
            B::GV     GLOB
            B::REGEXP REGEXP
        );
        *JSON::PP::reftype = sub {
            my $r = shift;

            return undef unless length(ref($r));

            my $t = ref(B::svref_2object($r));

            return
                exists $tmap{$t} ? $tmap{$t}
              : length(ref($$r)) ? 'REF'
              :                    'SCALAR';
        };
        *JSON::PP::refaddr = sub {
          return undef unless length(ref($_[0]));

          my $addr;
          if(defined(my $pkg = blessed($_[0]))) {
            $addr .= bless $_[0], 'Scalar::Util::Fake';
            bless $_[0], $pkg;
          }
          else {
            $addr .= $_[0]
          }

          $addr =~ /0x(\w+)/;
          local $^W;
          #no warnings 'portable';
          hex($1);
        }
    }
}


# shamely copied and modified from JSON::XS code.

$JSON::PP::true  = do { bless \(my $dummy = 1), "JSON::backportPP::Boolean" };
$JSON::PP::false = do { bless \(my $dummy = 0), "JSON::backportPP::Boolean" };

sub is_bool { defined $_[0] and UNIVERSAL::isa($_[0], "JSON::PP::Boolean"); }

sub true  { $JSON::PP::true  }
sub false { $JSON::PP::false }
sub null  { undef; }

###############################

package JSON::backportPP::Boolean;

@JSON::backportPP::Boolean::ISA = ('JSON::PP::Boolean');
use overload (
   "0+"     => sub { ${$_[0]} },
   "++"     => sub { $_[0] = ${$_[0]} + 1 },
   "--"     => sub { $_[0] = ${$_[0]} - 1 },
   fallback => 1,
);


###############################

package
    JSON::PP::IncrParser;

use strict;

use constant INCR_M_WS   => 0; # initial whitespace skipping
use constant INCR_M_STR  => 1; # inside string
use constant INCR_M_BS   => 2; # inside backslash
use constant INCR_M_JSON => 3; # outside anything, count nesting
use constant INCR_M_C0   => 4;
use constant INCR_M_C1   => 5;

$JSON::PP::IncrParser::VERSION = '1.01';

my $unpack_format = $] < 5.006 ? 'C*' : 'U*';

sub new {
    my ( $class ) = @_;

    bless {
        incr_nest    => 0,
        incr_text    => undef,
        incr_parsing => 0,
        incr_p       => 0,
    }, $class;
}


sub incr_parse {
    my ( $self, $coder, $text ) = @_;

    $self->{incr_text} = '' unless ( defined $self->{incr_text} );

    if ( defined $text ) {
        if ( utf8::is_utf8( $text ) and !utf8::is_utf8( $self->{incr_text} ) ) {
            utf8::upgrade( $self->{incr_text} ) ;
            utf8::decode( $self->{incr_text} ) ;
        }
        $self->{incr_text} .= $text;
    }


    my $max_size = $coder->get_max_size;

    if ( defined wantarray ) {

        $self->{incr_mode} = INCR_M_WS unless defined $self->{incr_mode};

        if ( wantarray ) {
            my @ret;

            $self->{incr_parsing} = 1;

            do {
                push @ret, $self->_incr_parse( $coder, $self->{incr_text} );

                unless ( !$self->{incr_nest} and $self->{incr_mode} == INCR_M_JSON ) {
                    $self->{incr_mode} = INCR_M_WS if $self->{incr_mode} != INCR_M_STR;
                }

            } until ( length $self->{incr_text} >= $self->{incr_p} );

            $self->{incr_parsing} = 0;

            return @ret;
        }
        else { # in scalar context
            $self->{incr_parsing} = 1;
            my $obj = $self->_incr_parse( $coder, $self->{incr_text} );
            $self->{incr_parsing} = 0 if defined $obj; # pointed by Martin J. Evans
            return $obj ? $obj : undef; # $obj is an empty string, parsing was completed.
        }

    }

}


sub _incr_parse {
    my ( $self, $coder, $text, $skip ) = @_;
    my $p = $self->{incr_p};
    my $restore = $p;

    my @obj;
    my $len = length $text;

    if ( $self->{incr_mode} == INCR_M_WS ) {
        while ( $len > $p ) {
            my $s = substr( $text, $p, 1 );
            $p++ and next if ( 0x20 >= unpack($unpack_format, $s) );
            $self->{incr_mode} = INCR_M_JSON;
            last;
       }
    }

    while ( $len > $p ) {
        my $s = substr( $text, $p++, 1 );

        if ( $s eq '"' ) {
            if (substr( $text, $p - 2, 1 ) eq '\\' ) {
                next;
            }

            if ( $self->{incr_mode} != INCR_M_STR  ) {
                $self->{incr_mode} = INCR_M_STR;
            }
            else {
                $self->{incr_mode} = INCR_M_JSON;
                unless ( $self->{incr_nest} ) {
                    last;
                }
            }
        }

        if ( $self->{incr_mode} == INCR_M_JSON ) {

            if ( $s eq '[' or $s eq '{' ) {
                if ( ++$self->{incr_nest} > $coder->get_max_depth ) {
                    Carp::croak('json text or perl structure exceeds maximum nesting level (max_depth set too low?)');
                }
            }
            elsif ( $s eq ']' or $s eq '}' ) {
                last if ( --$self->{incr_nest} <= 0 );
            }
            elsif ( $s eq '#' ) {
                while ( $len > $p ) {
                    last if substr( $text, $p++, 1 ) eq "\n";
                }
            }

        }

    }

    $self->{incr_p} = $p;

    return if ( $self->{incr_mode} == INCR_M_STR and not $self->{incr_nest} );
    return if ( $self->{incr_mode} == INCR_M_JSON and $self->{incr_nest} > 0 );

    return '' unless ( length substr( $self->{incr_text}, 0, $p ) );

    local $Carp::CarpLevel = 2;

    $self->{incr_p} = $restore;
    $self->{incr_c} = $p;

    my ( $obj, $tail ) = $coder->PP_decode_json( substr( $self->{incr_text}, 0, $p ), 0x10000001 );

    $self->{incr_text} = substr( $self->{incr_text}, $p );
    $self->{incr_p} = 0;

    return $obj or '';
}


sub incr_text {
    if ( $_[0]->{incr_parsing} ) {
        Carp::croak("incr_text can not be called when the incremental parser already started parsing");
    }
    $_[0]->{incr_text};
}


sub incr_skip {
    my $self  = shift;
    $self->{incr_text} = substr( $self->{incr_text}, $self->{incr_c} );
    $self->{incr_p} = 0;
}


sub incr_reset {
    my $self = shift;
    $self->{incr_text}    = undef;
    $self->{incr_p}       = 0;
    $self->{incr_mode}    = 0;
    $self->{incr_nest}    = 0;
    $self->{incr_parsing} = 0;
}

###############################


1;
__END__
=pod

=head1 NAME

JSON::PP - JSON::XS compatible pure-Perl module.

=head1 SYNOPSIS

 use JSON::PP;

 # exported functions, they croak on error
 # and expect/generate UTF-8

 $utf8_encoded_json_text = encode_json $perl_hash_or_arrayref;
 $perl_hash_or_arrayref  = decode_json $utf8_encoded_json_text;

 # OO-interface

 $coder = JSON::PP->new->ascii->pretty->allow_nonref;
 
 $json_text   = $json->encode( $perl_scalar );
 $perl_scalar = $json->decode( $json_text );
 
 $pretty_printed = $json->pretty->encode( $perl_scalar ); # pretty-printing
 
 # Note that JSON version 2.0 and above will automatically use
 # JSON::XS or JSON::PP, so you should be able to just:
 
 use JSON;


=head1 VERSION

    2.27200

L<JSON::XS> 2.27 (~2.30) compatible.

=head1 DESCRIPTION

This module is L<JSON::XS> compatible pure Perl module.
(Perl 5.8 or later is recommended)

JSON::XS is the fastest and most proper JSON module on CPAN.
It is written by Marc Lehmann in C, so must be compiled and
installed in the used environment.

JSON::PP is a pure-Perl module and has compatibility to JSON::XS.


=head2 FEATURES

=over

=item * correct unicode handling

This module knows how to handle Unicode (depending on Perl version).

See to L<JSON::XS/A FEW NOTES ON UNICODE AND PERL> and L<UNICODE HANDLING ON PERLS>.


=item * round-trip integrity

When you serialise a perl data structure using only data types supported
by JSON and Perl, the deserialised data structure is identical on the Perl
level. (e.g. the string "2.0" doesn't suddenly become "2" just because
it looks like a number). There I<are> minor exceptions to this, read the
MAPPING section below to learn about those.


=item * strict checking of JSON correctness

There is no guessing, no generating of illegal JSON texts by default,
and only JSON is accepted as input by default (the latter is a security feature).
But when some options are set, loose chcking features are available.

=back

=head1 FUNCTIONAL INTERFACE

Some documents are copied and modified from L<JSON::XS/FUNCTIONAL INTERFACE>.

=head2 encode_json

    $json_text = encode_json $perl_scalar

Converts the given Perl data structure to a UTF-8 encoded, binary string.

This function call is functionally identical to:

    $json_text = JSON::PP->new->utf8->encode($perl_scalar)

=head2 decode_json

    $perl_scalar = decode_json $json_text

The opposite of C<encode_json>: expects an UTF-8 (binary) string and tries
to parse that as an UTF-8 encoded JSON text, returning the resulting
reference.

This function call is functionally identical to:

    $perl_scalar = JSON::PP->new->utf8->decode($json_text)

=head2 JSON::PP::is_bool

    $is_boolean = JSON::PP::is_bool($scalar)

Returns true if the passed scalar represents either JSON::PP::true or
JSON::PP::false, two constants that act like C<1> and C<0> respectively
and are also used to represent JSON C<true> and C<false> in Perl strings.

=head2 JSON::PP::true

Returns JSON true value which is blessed object.
It C<isa> JSON::PP::Boolean object.

=head2 JSON::PP::false

Returns JSON false value which is blessed object.
It C<isa> JSON::PP::Boolean object.

=head2 JSON::PP::null

Returns C<undef>.

See L<MAPPING>, below, for more information on how JSON values are mapped to
Perl.


=head1 HOW DO I DECODE A DATA FROM OUTER AND ENCODE TO OUTER

This section supposes that your perl vresion is 5.8 or later.

If you know a JSON text from an outer world - a network, a file content, and so on,
is encoded in UTF-8, you should use C<decode_json> or C<JSON> module object
with C<utf8> enable. And the decoded result will contain UNICODE characters.

  # from network
  my $json        = JSON::PP->new->utf8;
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
Instead of them, you use C<JSON> module object with C<utf8> disable.

  $perl_scalar = $json->utf8(0)->decode( $unicode_json_text );

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
Instead of them, you use C<JSON> module object with C<utf8> disable.
Note that the resulted text is a UNICODE string but no problem to print it.

  # $perl_scalar contains $encoding encoded string values
  $unicode_json_text = $json->utf8(0)->encode( $perl_scalar );
  # $unicode_json_text consists of characters less than 0x100
  print $unicode_json_text;

Or C<decode $encoding> all string values and C<encode_json>:

  $perl_scalar->{ foo } = decode( $encoding, $perl_scalar->{ foo } );
  # ... do it to each string values, then encode_json
  $json_text = encode_json( $perl_scalar );

This method is a proper way but probably not efficient.

See to L<Encode>, L<perluniintro>.


=head1 METHODS

Basically, check to L<JSON> or L<JSON::XS>.

=head2 new

    $json = JSON::PP->new

Rturns a new JSON::PP object that can be used to de/encode JSON
strings.

All boolean flags described below are by default I<disabled>.

The mutators for flags all return the JSON object again and thus calls can
be chained:

   my $json = JSON::PP->new->utf8->space_after->encode({a => [1,2]})
   => {"a": [1, 2]}

=head2 ascii

    $json = $json->ascii([$enable])
    
    $enabled = $json->get_ascii

If $enable is true (or missing), then the encode method will not generate characters outside
the code range 0..127. Any Unicode characters outside that range will be escaped using either
a single \uXXXX or a double \uHHHH\uLLLLL escape sequence, as per RFC4627.
(See to L<JSON::XS/OBJECT-ORIENTED INTERFACE>).

In Perl 5.005, there is no character having high value (more than 255).
See to L<UNICODE HANDLING ON PERLS>.

If $enable is false, then the encode method will not escape Unicode characters unless
required by the JSON syntax or other flags. This results in a faster and more compact format.

  JSON::PP->new->ascii(1)->encode([chr 0x10401])
  => ["\ud801\udc01"]

=head2 latin1

    $json = $json->latin1([$enable])
    
    $enabled = $json->get_latin1

If $enable is true (or missing), then the encode method will encode the resulting JSON
text as latin1 (or iso-8859-1), escaping any characters outside the code range 0..255.

If $enable is false, then the encode method will not escape Unicode characters
unless required by the JSON syntax or other flags.

  JSON::XS->new->latin1->encode (["\x{89}\x{abc}"]
  => ["\x{89}\\u0abc"]    # (perl syntax, U+abc escaped, U+89 not)

See to L<UNICODE HANDLING ON PERLS>.

=head2 utf8

    $json = $json->utf8([$enable])
    
    $enabled = $json->get_utf8

If $enable is true (or missing), then the encode method will encode the JSON result
into UTF-8, as required by many protocols, while the decode method expects to be handled
an UTF-8-encoded string. Please note that UTF-8-encoded strings do not contain any
characters outside the range 0..255, they are thus useful for bytewise/binary I/O.

(In Perl 5.005, any character outside the range 0..255 does not exist.
See to L<UNICODE HANDLING ON PERLS>.)

In future versions, enabling this option might enable autodetection of the UTF-16 and UTF-32
encoding families, as described in RFC4627.

If $enable is false, then the encode method will return the JSON string as a (non-encoded)
Unicode string, while decode expects thus a Unicode string. Any decoding or encoding
(e.g. to UTF-8 or UTF-16) needs to be done yourself, e.g. using the Encode module.

Example, output UTF-16BE-encoded JSON:

  use Encode;
  $jsontext = encode "UTF-16BE", JSON::PP->new->encode ($object);

Example, decode UTF-32LE-encoded JSON:

  use Encode;
  $object = JSON::PP->new->decode (decode "UTF-32LE", $jsontext);


=head2 pretty

    $json = $json->pretty([$enable])

This enables (or disables) all of the C<indent>, C<space_before> and
C<space_after> flags in one call to generate the most readable
(or most compact) form possible.

Equivalent to:

   $json->indent->space_before->space_after

=head2 indent

    $json = $json->indent([$enable])
    
    $enabled = $json->get_indent

The default indent space length is three.
You can use C<indent_length> to change the length.

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

If you want your own sorting routine, you can give a code referece
or a subroutine name to C<sort_by>. See to C<JSON::PP OWN METHODS>.

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

   JSON::PP->new->allow_nonref->encode ("Hello, World!")
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

   my $js = JSON::PP->new->filter_json_object (sub { 5 });
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
   JSON::PP
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

In JSON::XS, this flag resizes strings generated by either
C<encode> or C<decode> to their minimum size possible.
It will also try to downgrade any strings to octet-form if possible.

In JSON::PP, it is noop about resizing strings but tries
C<utf8::downgrade> to the returned string by C<encode>.
See to L<utf8>.

See to L<JSON::XS/OBJECT-ORIENTED INTERFACE>

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

See L<JSON::XS/SSECURITY CONSIDERATIONS> for more info on why this is useful.

When a large value (100 or more) was set and it de/encodes a deep nested object/text,
it may raise a warning 'Deep recursion on subroutin' at the perl runtime phase.

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

See L<JSON::XS/SSECURITY CONSIDERATIONS> for more info on why this is useful.

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

=head1 INCREMENTAL PARSING

Most of this section are copied and modified from L<JSON::XS/INCREMENTAL PARSING>.

In some cases, there is the need for incremental parsing of JSON texts.
This module does allow you to parse a JSON stream incrementally.
It does so by accumulating text until it has a full JSON object, which
it then can decode. This process is similar to using C<decode_prefix>
to see if a full JSON object is available, but is much more efficient
(and can be implemented with a minimum of method calls).

This module will only attempt to parse the JSON text once it is sure it
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


=head1 JSON::PP OWN METHODS

=head2 allow_singlequote

    $json = $json->allow_singlequote([$enable])

If C<$enable> is true (or missing), then C<decode> will accept
JSON strings quoted by single quotations that are invalid JSON
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

See to L<JSON::XS/MAPPING> aboout the normal conversion of JSON number.

=head2 loose

    $json = $json->loose([$enable])

The unescaped [\x00-\x1f\x22\x2f\x5c] strings are invalid in JSON strings
and the module doesn't allow to C<decode> to these (except for \x2f).
If C<$enable> is true (or missing), then C<decode>  will accept these
unescaped strings.

    $json->loose->decode(qq|["abc
                                   def"]|);

See L<JSON::XS/SSECURITY CONSIDERATIONS>.

=head2 escape_slash

    $json = $json->escape_slash([$enable])

According to JSON Grammar, I<slash> (U+002F) is escaped. But default
JSON::PP (as same as JSON::XS) encodes strings without escaping slash.

If C<$enable> is true (or missing), then C<encode> will escape slashes.

=head2 indent_length

    $json = $json->indent_length($length)

JSON::XS indent space length is 3 and cannot be changed.
JSON::PP set the indent space length with the given $length.
The default is 3. The acceptable range is 0 to 15.

=head2 sort_by

    $json = $json->sort_by($function_name)
    $json = $json->sort_by($subroutine_ref)

If $function_name or $subroutine_ref are set, its sort routine are used
in encoding JSON objects.

   $js = $pc->sort_by(sub { $JSON::PP::a cmp $JSON::PP::b })->encode($obj);
   # is($js, q|{"a":1,"b":2,"c":3,"d":4,"e":5,"f":6,"g":7,"h":8,"i":9}|);

   $js = $pc->sort_by('own_sort')->encode($obj);
   # is($js, q|{"a":1,"b":2,"c":3,"d":4,"e":5,"f":6,"g":7,"h":8,"i":9}|);

   sub JSON::PP::own_sort { $JSON::PP::a cmp $JSON::PP::b }

As the sorting routine runs in the JSON::PP scope, the given
subroutine name and the special variables C<$a>, C<$b> will begin
'JSON::PP::'.

If $integer is set, then the effect is same as C<canonical> on.

=head1 INTERNAL

For developers.

=over

=item PP_encode_box

Returns

        {
            depth        => $depth,
            indent_count => $indent_count,
        }


=item PP_decode_box

Returns

        {
            text    => $text,
            at      => $at,
            ch      => $ch,
            len     => $len,
            depth   => $depth,
            encoding      => $encoding,
            is_valid_utf8 => $is_valid_utf8,
        };

=back

=head1 MAPPING

This section is copied from JSON::XS and modified to C<JSON::PP>.
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

When C<allow_bignum> is enable, the big integers 
and the numeric can be optionally converted into L<Math::BigInt> and
L<Math::BigFloat> objects.

=item true, false

These JSON atoms become C<JSON::PP::true> and C<JSON::PP::false>,
respectively. They are overloaded to act almost exactly like the numbers
C<1> and C<0>. You can check wether a scalar is a JSON boolean by using
the C<JSON::is_bool> function.

   print JSON::PP::true . "\n";
    => true
   print JSON::PP::true + 1;
    => 1

   ok(JSON::true eq  '1');
   ok(JSON::true == 1);

C<JSON> will install these missing overloading features to the backend modules.


=item null

A JSON null atom becomes C<undef> in Perl.

C<JSON::PP::null> returns C<unddef>.

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


=item array references

Perl array references become JSON arrays.

=item other references

Other unblessed references are generally not allowed and will cause an
exception to be thrown, except for references to the integers C<0> and
C<1>, which get turned into C<false> and C<true> atoms in JSON. You can
also use C<JSON::false> and C<JSON::true> to improve readability.

   to_json [\0,JSON::PP::true]      # yields [false,true]

=item JSON::PP::true, JSON::PP::false, JSON::PP::null

These special values become JSON true and JSON false values,
respectively. You can also use C<\1> and C<\0> directly if you want.

JSON::PP::null returns C<undef>.

=item blessed objects

Blessed objects are not directly representable in JSON. See the
C<allow_blessed> and C<convert_blessed> methods on various options on
how to deal with this: basically, you can choose between throwing an
exception, encoding the reference as if it weren't blessed, or provide
your own serialiser method.

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

When C<allow_bignum> is enable, 
C<encode> converts C<Math::BigInt> objects and C<Math::BigFloat>
objects into JSON numbers.


=back

=head1 UNICODE HANDLING ON PERLS

If you do not know about Unicode on Perl well,
please check L<JSON::XS/A FEW NOTES ON UNICODE AND PERL>.

=head2 Perl 5.8 and later

Perl can handle Unicode and the JSON::PP de/encode methods also work properly.

    $json->allow_nonref->encode(chr hex 3042);
    $json->allow_nonref->encode(chr hex 12345);

Reuturns C<"\u3042"> and C<"\ud808\udf45"> respectively.

    $json->allow_nonref->decode('"\u3042"');
    $json->allow_nonref->decode('"\ud808\udf45"');

Returns UTF-8 encoded strings with UTF8 flag, regarded as C<U+3042> and C<U+12345>.

Note that the versions from Perl 5.8.0 to 5.8.2, Perl built-in C<join> was broken,
so JSON::PP wraps the C<join> with a subroutine. Thus JSON::PP works slow in the versions.


=head2 Perl 5.6

Perl can handle Unicode and the JSON::PP de/encode methods also work.

=head2 Perl 5.005

Perl 5.005 is a byte sementics world -- all strings are sequences of bytes.
That means the unicode handling is not available.

In encoding,

    $json->allow_nonref->encode(chr hex 3042);  # hex 3042 is 12354.
    $json->allow_nonref->encode(chr hex 12345); # hex 12345 is 74565.

Returns C<B> and C<E>, as C<chr> takes a value more than 255, it treats
as C<$value % 256>, so the above codes are equivalent to :

    $json->allow_nonref->encode(chr 66);
    $json->allow_nonref->encode(chr 69);

In decoding,

    $json->decode('"\u00e3\u0081\u0082"');

The returned is a byte sequence C<0xE3 0x81 0x82> for UTF-8 encoded
japanese character (C<HIRAGANA LETTER A>).
And if it is represented in Unicode code point, C<U+3042>.

Next, 

    $json->decode('"\u3042"');

We ordinary expect the returned value is a Unicode character C<U+3042>.
But here is 5.005 world. This is C<0xE3 0x81 0x82>.

    $json->decode('"\ud808\udf45"');

This is not a character C<U+12345> but bytes - C<0xf0 0x92 0x8d 0x85>.


=head1 TODO

=over

=item speed

=item memory saving

=back


=head1 SEE ALSO

Most of the document are copied and modified from JSON::XS doc.

L<JSON::XS>

RFC4627 (L<http://www.ietf.org/rfc/rfc4627.txt>)

=head1 AUTHOR

Makamaka Hannyaharamitu, E<lt>makamaka[at]cpan.orgE<gt>


=head1 COPYRIGHT AND LICENSE

Copyright 2007-2011 by Makamaka Hannyaharamitu

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself. 

=cut
