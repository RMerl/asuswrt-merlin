use lib "$Bin/lib";
use JSON;

@ARGV < 2 and die "Usage: $0 <prefix> <file>\n";
my $prefix = shift @ARGV;

our $ctl;
our %tlv_types = (
	gint8 => "int8_t",
	guint8 => "uint8_t",
	gint16 => "int16_t",
	guint16 => "uint16_t",
	gint32 => "int32_t",
	guint32 => "uint32_t",
	gint64 => "int64_t",
	guint64 => "uint64_t",
	gboolean => "bool",
);

$prefix eq 'ctl_' and $ctl = 1;

sub get_json() {
	local $/;
	my $json = <>;
	$json =~ s/^\s*\/\/.*$//mg;
	return decode_json($json);
}

sub gen_cname($) {
	my $name = shift;

	$name =~ s/[^a-zA-Z0-9_]/_/g;
	return lc($name);
}

sub gen_has_types($) {
	my $data = shift;

	foreach my $field (@$data) {
		my $type = $field->{"format"};
		$type and return 1;
	}
	return undef
}

sub gen_tlv_set_func($$) {
	my $name = shift;
	my $data = shift;

	$name = gen_cname($name);
	if (gen_has_types($data)) {
		return "int qmi_set_$name(struct qmi_msg *msg, struct qmi_$name *req)"
	} else {
		return "int qmi_set_$name(struct qmi_msg *msg)"
	}
}

sub gen_tlv_parse_func($$) {
	my $name = shift;
	my $data = shift;

	$name = gen_cname($name);
	if (gen_has_types($data)) {
		return "int qmi_parse_$name(struct qmi_msg *msg, struct qmi_$name *res)"
	} else {
		return "int qmi_parse_$name(struct qmi_msg *msg)"
	}
}

sub gen_foreach_message_type($$$)
{
	my $data = shift;
	my $req_sub = shift;
	my $res_sub = shift;

	foreach my $entry (@$data) {
		my $args = [];
		my $fields = [];

		next if $entry->{type} ne 'Message';
		next if not defined $entry->{input} and not defined $entry->{output};

		&$req_sub($prefix.$entry->{name}." Request", $entry->{input}, $entry);
		&$res_sub($prefix.$entry->{name}." Response", $entry->{output}, $entry);
	}
}

1;
