#!/usr/bin/env perl
use strict;

use FindBin '$Bin';
require "$Bin/gen-common.pm";

our %tlv_types;

my $data = get_json();

sub gen_tlv_type($$$) {
	my $cname = shift;
	my $elem = shift;
	my $indent = shift;

	my $type = $elem->{"format"};
	my $ptype = $elem->{"public-format"};
	my $data;

	$type or return undef;
	$ptype or $ptype = $type;

	if ($type eq "guint-sized") {
		my $size = $elem->{"guint-size"};

		if ($size > 4 and $size < 8) {
			$ptype = "guint64";
		} elsif ($size > 2) {
			$ptype = "guint32";
		} else {
			die "Invalid size for guint-sized";
		}
	}

	if ($tlv_types{$ptype}) {
		return $indent.$tlv_types{$ptype}." $cname;";
	} elsif ($tlv_types{$type}) {
		return $indent."$ptype $cname;";
	} elsif ($type eq "string") {
		return $indent."char *$cname;", 1;
	} elsif ($type eq "array") {
		if ($elem->{"fixed-size"}) {
			my $len_f = '['.$elem->{"fixed-size"}.']';
			return gen_tlv_type("$cname$len_f", $elem->{"array-element"}, $indent);
		}
		my ($type, $no_set_field) = gen_tlv_type("*$cname", $elem->{"array-element"}, $indent);
		return undef if not defined $type;
		return $indent."unsigned int $cname\_n;$type", 1;
	} elsif ($type eq "sequence" or $type eq "struct") {
		my $contents = $elem->{"contents"};
		my $data = "struct {";

		foreach my $field (@$contents) {
			my $_cname = gen_cname($field->{name});
			my ($_data, $no_set_field) = gen_tlv_type($_cname, $field, "$indent\t");
			$data .= $_data;
		}
		return $indent.$data.$indent."} $cname;";
	} else {
		die "Unknown type: $ptype\n";
	}
}

sub gen_tlv_struct($$) {
	my $name = shift;
	my $data = shift;
	my $_set = "";
	my $_data = "";

	foreach my $field (@$data) {
		my $cname = gen_cname($field->{name});
		my ($data, $no_set_field) = gen_tlv_type($cname, $field, "\n\t\t");

		next if not defined $data;
		$_data .= $data;

		next if $no_set_field;
		$_set .= "\n\t\tunsigned int $cname : 1;";
	}

	$name = gen_cname($name);

	$_data or return;

	$_set .= "\n\t";
	$_data .= "\n\t";

	print <<EOF
struct qmi_$name {
	struct {$_set} set;
	struct {$_data} data;
};

EOF
}

sub gen_set_func_header($$)
{
	my $name = shift;
	my $data = shift;

	my $func = gen_tlv_set_func($name, $data);
	$func and print "$func;\n";
}

sub gen_parse_func_header($$)
{
	my $name = shift;
	my $data = shift;

	my $func = gen_tlv_parse_func($name, $data);
	$func and print "$func;\n\n";
}

gen_foreach_message_type($data, \&gen_tlv_struct, \&gen_tlv_struct);
gen_foreach_message_type($data, \&gen_set_func_header, \&gen_parse_func_header);
