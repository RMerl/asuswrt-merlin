#!/usr/bin/env perl
use strict;

use FindBin '$Bin';
require "$Bin/gen-common.pm";

our %tlv_types;
our $ctl;

my $data = get_json();
my $varsize_field;

my %tlv_get = (
	gint8 => "*(int8_t *) get_next(1)",
	guint8 => "*(uint8_t *) get_next(1)",
	gint16 => "le16_to_cpu(*(uint16_t *) get_next(2))",
	guint16 => "le16_to_cpu(*(uint16_t *) get_next(2))",
	gint32 => "le32_to_cpu(*(uint32_t *) get_next(4))",
	guint32 => "le32_to_cpu(*(uint32_t *) get_next(4))",
	gint64 => "le64_to_cpu(*(uint64_t *) get_next(8))",
	guint64 => "le64_to_cpu(*(uint64_t *) get_next(8))",
);

my %tlv_get_be = (
	gint16 => "be16_to_cpu(*(uint16_t *) get_next(2))",
	guint16 => "be16_to_cpu(*(uint16_t *) get_next(2))",
	gint32 => "be32_to_cpu(*(uint32_t *) get_next(4))",
	guint32 => "be32_to_cpu(*(uint32_t *) get_next(4))",
	gint64 => "be64_to_cpu(*(uint64_t *) get_next(8))",
	guint64 => "be64_to_cpu(*(uint64_t *) get_next(8))",
);

sub gen_tlv_parse_field($$$$) {
	my $var = shift;
	my $elem = shift;
	my $n_indent = shift;
	my $iterator = shift;
	my $data = "";

	my $indent = "\t" x ($n_indent + 3);
	my $use_iterator = 0;
	my $field = 0;

	my $type = $elem->{"format"};

	$varsize_field and die "Cannot place fields after a variable-sized field (var: $var, field: $varsize_field)\n";

	my $val;
	if ($elem->{endian} eq 'network') {
		$val = $tlv_get_be{$type};
	} else {
		$val = $tlv_get{$type};
	}

	if ($val) {
		return $indent."$var = $val;\n";
	} elsif ($type eq "array") {
		my $size;
		my $cur_varsize_field;
		my $var_data;
		my $var_iterator;

		if ($elem->{"fixed-size"}) {
			$size = $elem->{"fixed-size"};
			$data .= $indent."for ($iterator = 0; $iterator < $size; $iterator\++) {\n";

			($var_data, $var_iterator) =
				gen_tlv_parse_field($var."[$iterator]", $elem->{"array-element"}, $n_indent + 1, "i$iterator");

		} else {
			my $prefix = $elem->{"size-prefix-format"};
			$prefix or $prefix = 'guint8';

			$size = $tlv_get{$prefix};
			die "Unknown size element type '$prefix'" if not defined $size;

			($var_data, $var_iterator) =
				gen_tlv_parse_field($var."[$var\_n]", $elem->{"array-element"}, $n_indent + 1, "i$iterator");

			$var_data .= $indent."\t$var\_n++;\n";
			$data .= $indent."$iterator = $size;\n";
			$data .= $indent."$var = __qmi_alloc_static($iterator * sizeof($var\[0]));\n";
			$data .= $indent."while($iterator\-- > 0) {\n";
		}

		$var_iterator and $data .= $indent."\tunsigned int i$iterator;\n";
		$data .= $var_data;
		$data .= $indent."}\n";

		$varsize_field = $cur_varsize_field;

		return $data, 1;
	} elsif ($type eq "struct" or $type eq "sequence") {
		foreach my $field (@{$elem->{contents}}) {
			my $field_cname = gen_cname($field->{name});
			my ($var_data, $var_iterator) =
				gen_tlv_parse_field("$var.$field_cname", $field, $n_indent, $iterator);

			$data .= $var_data;
			$var_iterator and $use_iterator = 1;
		}
		return $data, $use_iterator;
	} elsif ($type eq "string") {
		my $size = $elem->{"fixed-size"};
		$size or do {
			my $prefix = $elem->{"size-prefix-format"};
			$prefix or do {
				$elem->{type} eq 'TLV' or $prefix = 'guint8';
			};

			if ($prefix) {
				$size = $tlv_get{$prefix};
			} else {
				$size = "cur_tlv_len - ofs";
				$varsize_field = $var;
			}
		};

		$data .= $indent."$iterator = $size;\n";
		my $maxsize = $elem->{"max-size"};
		$maxsize and do {
			$data .= $indent."if ($iterator > $maxsize)\n";
			$data .= $indent."\t$iterator = $maxsize;\n";
		};
		$data .= $indent.$var." = __qmi_copy_string(get_next($iterator), $iterator);\n";
		return $data, 1;
	} elsif ($type eq "guint-sized") {
		my $size = $elem->{"guint-size"};
		return $indent."$var = ({ uint64_t var; memcpy(&var, get_next($size), $size); le64_to_cpu(var); });\n";
	} else {
		die "Invalid type $type for variable $var";
	}
}

sub gen_tlv_type($$$) {
	my $cname = shift;
	my $elem = shift;
	my $idx = shift;
	my $idx_word = "found[".int($idx / 32)."]";
	my $idx_bit = "(1 << ".($idx % 32).")";

	my $type = $elem->{"format"};
	my $id = $elem->{"id"};
	my $data = "";
	undef $varsize_field;
	my $indent = "\t\t\t";

	$type or return undef;

	print <<EOF;
		case $id:
			if ($idx_word & $idx_bit)
				break;

			$idx_word |= $idx_bit;
EOF

	my $val = $tlv_get{$type};
	if ($val) {
		print $indent."qmi_set(res, $cname, $val);\n";
	} elsif ($type eq "string") {
		my ($var_data, $var_iterator) =
			gen_tlv_parse_field("res->data.$cname", $elem, 0, "i");
		print "$var_data";
	} elsif ($type eq "array") {
		$elem->{"fixed-size"} and $data = $indent."res->set.$cname = 1;\n";
		my ($var_data, $var_iterator) =
			gen_tlv_parse_field("res->data.$cname", $elem, 0, "i");
		print "$data$var_data\n";
	} elsif ($type eq "sequence" or $type eq "struct") {
		my ($var_data, $var_iterator) =
			gen_tlv_parse_field("res->data.$cname", $elem, 0, "i");

		print $indent."res->set.$cname = 1;\n".$var_data;
	}
	print <<EOF;
			break;

EOF
}

sub gen_parse_func($$)
{
	my $name = shift;
	my $data = shift;

	my $type = "svc";
	$ctl and $type = "ctl";

	print gen_tlv_parse_func($name, $data)."\n";
	print <<EOF;
{
	void *tlv_buf = &msg->$type.tlv;
	unsigned int tlv_len = le16_to_cpu(msg->$type.tlv_len);
EOF

	if (gen_has_types($data)) {
		my $n_bits = scalar @$data;
		my $n_words = int(($n_bits + 31) / 32);
		my $i = 0;

		print <<EOF;
	struct tlv *tlv;
	int i;
	uint32_t found[$n_words] = {};

	memset(res, 0, sizeof(*res));

	__qmi_alloc_reset();
	while ((tlv = tlv_get_next(&tlv_buf, &tlv_len)) != NULL) {
		unsigned int cur_tlv_len = le16_to_cpu(tlv->len);
		unsigned int ofs = 0;

		switch(tlv->type) {
EOF
		foreach my $field (@$data) {
			my $cname = gen_cname($field->{name});
			gen_tlv_type($cname, $field, $i++);
		}

		print <<EOF;
		default:
			break;
		}
	}

	return 0;

error_len:
	fprintf(stderr, "%s: Invalid TLV length in message, tlv=0x%02x, len=%d\\n",
	        __func__, tlv->type, le16_to_cpu(tlv->len));
	return QMI_ERROR_INVALID_DATA;
EOF
	} else {
		print <<EOF;

	return qmi_check_message_status(tlv_buf, tlv_len);
EOF
	}

	print <<EOF;
}

EOF
}

my %tlv_set = (
	guint8 => sub { my $a = shift; my $b = shift; print "*(uint8_t *) $a = $b;\n" },
	guint16 => sub { my $a = shift; my $b = shift; print "*(uint16_t *) $a = cpu_to_le16($b);\n" },
	guint32 => sub { my $a = shift; my $b = shift; print "*(uint32_t *) $a = cpu_to_le32($b);\n" },
);

my %tlv_put = (
	gint8 => sub { my $a = shift; "put_tlv_var(uint8_t, $a, 1);\n" },
	guint8 => sub { my $a = shift; "put_tlv_var(uint8_t, $a, 1);\n" },
	gint16 => sub { my $a = shift; "put_tlv_var(uint16_t, cpu_to_le16($a), 2);\n" },
	guint16 => sub { my $a = shift; "put_tlv_var(uint16_t, cpu_to_le16($a), 2);\n" },
	gint32 => sub { my $a = shift; "put_tlv_var(uint32_t, cpu_to_le32($a), 4);\n" },
	guint32 => sub { my $a = shift; "put_tlv_var(uint32_t, cpu_to_le32($a), 4);\n" },
	gint64 => sub { my $a = shift; "put_tlv_var(uint64_t, cpu_to_le64($a), 8);\n" },
	guint64 => sub { my $a = shift; "put_tlv_var(uint64_t, cpu_to_le64($a), 8);\n" },
);

my %tlv_put_be = (
	gint16 => sub { my $a = shift; "put_tlv_var(uint16_t, cpu_to_be16($a), 2);\n" },
	guint16 => sub { my $a = shift; "put_tlv_var(uint16_t, cpu_to_be16($a), 2);\n" },
	gint32 => sub { my $a = shift; "put_tlv_var(uint32_t, cpu_to_be32($a), 4);\n" },
	guint32 => sub { my $a = shift; "put_tlv_var(uint32_t, cpu_to_be32($a), 4);\n" },
	gint64 => sub { my $a = shift; "put_tlv_var(uint64_t, cpu_to_be64($a), 8);\n" },
	guint64 => sub { my $a = shift; "put_tlv_var(uint64_t, cpu_to_be64($a), 8);\n" },
);

sub gen_tlv_val_set($$$$$)
{
	my $cname = shift;
	my $elem = shift;
	my $indent = shift;
	my $iterator = shift;
	my $cond = shift;
	my $prev_cond;

	my $type = $elem->{format};
	my $data = "";

	my $put;
	if ($elem->{endian} eq 'network') {
		$put = $tlv_put_be{$type};
	} else {
		$put = $tlv_put{$type};
	}
	$put and return $indent.&$put($cname);

	$type eq 'array' and do {
		my $size = $elem->{"fixed-size"};

		$size or do {
			$cond and $$cond = $cname;
			$size = $cname."_n";

			my $prefix = $elem->{"size-prefix-format"};
			$prefix or $prefix = 'gint8';

			$put = $tlv_put{$prefix};
			$put or die "Unknown size prefix type $prefix\n";

			$data .= $indent.&$put($size);
		};

		$data .= $indent."for ($iterator = 0; $iterator < $size; $iterator++) {\n";
		my ($var_data, $var_iterator) =
			gen_tlv_val_set($cname."[$iterator]", $elem->{"array-element"}, "$indent\t", "i$iterator", undef);

		$var_iterator and $data .= $indent."\tunsigned int i$iterator;\n";
		$data .= $var_data;
		$data .= $indent."}\n";

		return $data, 1;
	};

	$type eq 'string' and do {
		$cond and $$cond = $cname;

		my $len = $elem->{"fixed-size"};
		$len or $len = "strlen($cname)";

		$data .= $indent."$iterator = $len;\n";

		$len = $elem->{"max-size"};
		$len and do {
			$data .= $indent."if ($iterator > $len)\n";
			$data .= $indent."\t$iterator = $len;\n";
		};

		my $prefix = $elem->{"size-prefix-format"};
		$prefix or do {
			$elem->{"type"} eq 'TLV' or $prefix = 'guint8';
		};

		$prefix and do {
			my $put = $tlv_put{$prefix} or die "Unknown size prefix format $prefix";
			$data .= $indent.&$put("$iterator");
		};

		$data .= $indent."strncpy(__qmi_alloc_static($iterator), $cname, $iterator);\n";

		return $data, 1;
	};

	($type eq 'sequence' or $type eq 'struct') and do {
		my $use_iterator;

		foreach my $field (@{$elem->{contents}}) {
		my $field_cname = gen_cname($field->{name});
			my ($var_data, $var_iterator) =
				gen_tlv_val_set("$cname.$field_cname", $field, $indent, $iterator, undef);

			$var_iterator and $use_iterator = 1;
			$data .= $var_data;
		}
		return $data, $use_iterator;
	};

	die "Unknown type $type";
}

sub gen_tlv_attr_set($$)
{
	my $cname = shift;
	my $elem = shift;
	my $indent = "\t";
	my $data = "";
	my $iterator = "";
	my $size_var = "";
	my $id = $elem->{id};

	my $cond = "req->set.$cname";
	my ($var_data, $use_iterator) =
		gen_tlv_val_set("req->data.$cname", $elem, "\t\t", "i", \$cond);
	$use_iterator and $iterator = "\t\tunsigned int i;\n";

	$data = <<EOF;
	if ($cond) {
		void *buf;
		unsigned int ofs;
$iterator$size_var
		__qmi_alloc_reset();
$var_data
		buf = __qmi_get_buf(&ofs);
		tlv_new(msg, $id, ofs, buf);
	}

EOF
	print "$data";
}

sub gen_set_func($$)
{
	my $name = shift;
	my $fields = shift;
	my $data = shift;

	my $type = "svc";
	my $service = $data->{service};
	my $id = $data->{id};

	$service eq 'CTL' and $type = 'ctl';

	print gen_tlv_set_func($name, $fields)."\n";
	print <<EOF;
{
	qmi_init_request_message(msg, QMI_SERVICE_$service);
	msg->$type.message = cpu_to_le16($id);

EOF
	foreach my $field (@$fields) {
		my $cname = gen_cname($field->{name});
		gen_tlv_attr_set($cname, $field);
	}

	print <<EOF;
	return 0;
}

EOF
}

print <<EOF;
/* generated by uqmi gen-code.pl */
#include <stdio.h>
#include <string.h>
#include "qmi-message.h"

#define get_next(_size) ({ void *_buf = &tlv->data[ofs]; ofs += _size; if (ofs > cur_tlv_len) goto error_len; _buf; })
#define copy_tlv(_val, _size) \\
	do { \\
		unsigned int __size = _size; \\
		if (__size > 0) \\
			memcpy(__qmi_alloc_static(__size), _val, __size); \\
	} while (0);

#define put_tlv_var(_type, _val, _size) \\
	do { \\
		_type __var = _val; \\
		copy_tlv(&__var, _size); \\
	} while(0)

EOF

gen_foreach_message_type($data, \&gen_set_func, \&gen_parse_func);
