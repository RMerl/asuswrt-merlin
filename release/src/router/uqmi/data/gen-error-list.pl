#!/usr/bin/env perl
use strict;
my $doc_start;
my $error_data;
my $line;
my @errors;

while ($line = <>) {
	chomp $line;

	$line =~ /^\/\*\*/ and do {
		$doc_start = 1;
		next;
	};

	$line =~ /^\s*\*\// and undef $error_data;

	$doc_start and $line =~ /^\s*\*\s*QmiProtocolError:/ and do {
		$error_data = 1;
		undef $doc_start;
		next;
	};
	undef $doc_start;

	$line =~ /^\s*\*\s*@(.+): (.+)\./ and push @errors, [ $1, $2 ];
}

@errors > 0 or die "No data found\n";

print <<EOF;
static const struct {
	QmiProtocolError code;
	const char *text;
} qmi_errors[] = {
EOF
foreach my $error (@errors) {
	print "\t{ ".$error->[0].", \"".$error->[1]."\" },\n";
}
print <<EOF;
};
EOF
