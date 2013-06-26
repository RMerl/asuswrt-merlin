###################################################
# utility functions to support pidl
# Copyright tridge@samba.org 2000
# released under the GNU GPL
package util;

#####################################################################
# load a data structure from a file (as saved with SaveStructure)
sub LoadStructure($)
{
	my $f = shift;
	my $contents = FileLoad($f);
	defined $contents || return undef;
	return eval "$contents";
}

use strict;

#####################################################################
# flatten an array of arrays into a single array
sub FlattenArray2($) 
{ 
    my $a = shift;
    my @b;
    for my $d (@{$a}) {
	for my $d1 (@{$d}) {
	    push(@b, $d1);
	}
    }
    return \@b;
}

#####################################################################
# flatten an array of arrays into a single array
sub FlattenArray($) 
{ 
    my $a = shift;
    my @b;
    for my $d (@{$a}) {
	for my $d1 (@{$d}) {
	    push(@b, $d1);
	}
    }
    return \@b;
}

#####################################################################
# flatten an array of hashes into a single hash
sub FlattenHash($) 
{ 
    my $a = shift;
    my %b;
    for my $d (@{$a}) {
	for my $k (keys %{$d}) {
	    $b{$k} = $d->{$k};
	}
    }
    return \%b;
}


#####################################################################
# traverse a perl data structure removing any empty arrays or
# hashes and any hash elements that map to undef
sub CleanData($)
{
    sub CleanData($);
    my($v) = shift;
    if (ref($v) eq "ARRAY") {
	foreach my $i (0 .. $#{$v}) {
	    CleanData($v->[$i]);
	    if (ref($v->[$i]) eq "ARRAY" && $#{$v->[$i]}==-1) { 
		    $v->[$i] = undef; 
		    next; 
	    }
	}
	# this removes any undefined elements from the array
	@{$v} = grep { defined $_ } @{$v};
    } elsif (ref($v) eq "HASH") {
	foreach my $x (keys %{$v}) {
	    CleanData($v->{$x});
	    if (!defined $v->{$x}) { delete($v->{$x}); next; }
	    if (ref($v->{$x}) eq "ARRAY" && $#{$v->{$x}}==-1) { delete($v->{$x}); next; }
	}
    }
}


#####################################################################
# return the modification time of a file
sub FileModtime($)
{
    my($filename) = shift;
    return (stat($filename))[9];
}


#####################################################################
# read a file into a string
sub FileLoad($)
{
    my($filename) = shift;
    local(*INPUTFILE);
    open(INPUTFILE, $filename) || return undef;
    my($saved_delim) = $/;
    undef $/;
    my($data) = <INPUTFILE>;
    close(INPUTFILE);
    $/ = $saved_delim;
    return $data;
}

#####################################################################
# write a string into a file
sub FileSave($$)
{
    my($filename) = shift;
    my($v) = shift;
    local(*FILE);
    open(FILE, ">$filename") || die "can't open $filename";    
    print FILE $v;
    close(FILE);
}

#####################################################################
# return a filename with a changed extension
sub ChangeExtension($$)
{
    my($fname) = shift;
    my($ext) = shift;
    if ($fname =~ /^(.*)\.(.*?)$/) {
	return "$1$ext";
    }
    return "$fname$ext";
}

#####################################################################
# a dumper wrapper to prevent dependence on the Data::Dumper module
# unless we actually need it
sub MyDumper($)
{
	require Data::Dumper;
	my $s = shift;
	return Data::Dumper::Dumper($s);
}

#####################################################################
# save a data structure into a file
sub SaveStructure($$)
{
	my($filename) = shift;
	my($v) = shift;
	FileSave($filename, MyDumper($v));
}

#####################################################################
# see if a pidl property list contains a give property
sub has_property($$)
{
	my($e) = shift;
	my($p) = shift;

	if (!defined $e->{PROPERTIES}) {
		return undef;
	}

	return $e->{PROPERTIES}->{$p};
}


sub is_scalar_type($)
{
    my($type) = shift;

    if ($type =~ /^u?int\d+/) {
	    return 1;
    }
    if ($type =~ /char|short|long|NTTIME|
	time_t|error_status_t|boolean32|unsigned32|
	HYPER_T|wchar_t|DATA_BLOB/x) {
	    return 1;
    }

    return 0;
}

# return the NDR alignment for a type
sub type_align($)
{
    my($e) = shift;
    my $type = $e->{TYPE};

    if (need_wire_pointer($e)) {
	    return 4;
    }

    return 4, if ($type eq "uint32");
    return 4, if ($type eq "long");
    return 2, if ($type eq "short");
    return 1, if ($type eq "char");
    return 1, if ($type eq "uint8");
    return 2, if ($type eq "uint16");
    return 4, if ($type eq "NTTIME");
    return 4, if ($type eq "time_t");
    return 8, if ($type eq "HYPER_T");
    return 2, if ($type eq "wchar_t");
    return 4, if ($type eq "DATA_BLOB");

    # it must be an external type - all we can do is guess 
    return 4;
}

# this is used to determine if the ndr push/pull functions will need
# a ndr_flags field to split by buffers/scalars
sub is_builtin_type($)
{
    my($type) = shift;

    return 1, if (is_scalar_type($type));

    return 0;
}

# determine if an element needs a reference pointer on the wire
# in its NDR representation
sub need_wire_pointer($)
{
	my $e = shift;
	if ($e->{POINTERS} && 
	    !has_property($e, "ref")) {
		return $e->{POINTERS};
	}
	return undef;
}

# determine if an element is a pass-by-reference structure
sub is_ref_struct($)
{
	my $e = shift;
	if (!is_scalar_type($e->{TYPE}) &&
	    has_property($e, "ref")) {
		return 1;
	}
	return 0;
}

# determine if an element is a pure scalar. pure scalars do not
# have a "buffers" section in NDR
sub is_pure_scalar($)
{
	my $e = shift;
	if (has_property($e, "ref")) {
		return 1;
	}
	if (is_scalar_type($e->{TYPE}) && 
	    !$e->{POINTERS} && 
	    !array_size($e)) {
		return 1;
	}
	return 0;
}

# determine the array size (size_is() or ARRAY_LEN)
sub array_size($)
{
	my $e = shift;
	my $size = has_property($e, "size_is");
	if ($size) {
		return $size;
	}
	$size = $e->{ARRAY_LEN};
	if ($size) {
		return $size;
	}
	return undef;
}

# see if a variable needs to be allocated by the NDR subsystem on pull
sub need_alloc($)
{
	my $e = shift;

	if (has_property($e, "ref")) {
		return 0;
	}

	if ($e->{POINTERS} || array_size($e)) {
		return 1;
	}

	return 0;
}

# determine the C prefix used to refer to a variable when passing to a push
# function. This will be '*' for pointers to scalar types, '' for scalar
# types and normal pointers and '&' for pass-by-reference structures
sub c_push_prefix($)
{
	my $e = shift;

	if ($e->{TYPE} =~ "string") {
		return "";
	}

	if (is_scalar_type($e->{TYPE}) &&
	    $e->{POINTERS}) {
		return "*";
	}
	if (!is_scalar_type($e->{TYPE}) &&
	    !$e->{POINTERS} &&
	    !array_size($e)) {
		return "&";
	}
	return "";
}


# determine the C prefix used to refer to a variable when passing to a pull
# return '&' or ''
sub c_pull_prefix($)
{
	my $e = shift;

	if (!$e->{POINTERS} && !array_size($e)) {
		return "&";
	}

	if ($e->{TYPE} =~ "string") {
		return "&";
	}

	return "";
}

# determine if an element has a direct buffers component
sub has_direct_buffers($)
{
	my $e = shift;
	if ($e->{POINTERS} || array_size($e)) {
		return 1;
	}
	return 0;
}

# return 1 if the string is a C constant
sub is_constant($)
{
	my $s = shift;
	if ($s =~ /^\d/) {
		return 1;
	}
	return 0;
}

# return 1 if this is a fixed array
sub is_fixed_array($)
{
	my $e = shift;
	my $len = $e->{"ARRAY_LEN"};
	if (defined $len && is_constant($len)) {
		return 1;
	}
	return 0;
}

# return 1 if this is a inline array
sub is_inline_array($)
{
	my $e = shift;
	my $len = $e->{"ARRAY_LEN"};
	if (is_fixed_array($e) ||
	    defined $len && $len ne "*") {
		return 1;
	}
	return 0;
}

1;

