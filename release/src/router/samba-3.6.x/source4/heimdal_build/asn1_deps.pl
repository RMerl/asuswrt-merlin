#!/usr/bin/perl
# Generate make dependency rules for asn1 files
# Jelmer Vernooij <jelmer@samba.org> 2005
# Andrew Bartlett <abartlet@samba.org> 2006-2009
# Stefan Metzmacher <metze@samba.org> 2007
# GPL

use File::Basename;

my $file = shift;
my $prefix = shift;
my $dirname = shift;
my $options = join(' ', @ARGV);
my $import;
my @imports = ();
my $dep;
my @deps = ();

$basename = basename($file);
if (not defined $options) {
    $options = "";
}

my $header = "$dirname/$prefix.h";
my $headerx = "$dirname/$prefix.hx";
my $headerpriv = "$dirname/$prefix-priv.h";
my $headerprivx = "$dirname/$prefix-priv.hx";
my $o_file = "$dirname/asn1_$prefix.o";
my $c_file = "$dirname/asn1_$prefix.c";
my $x_file = "$dirname/asn1_$prefix.x";
my $output_file = "$dirname/" . $prefix . "_asn1_files";

print "basics:: $header\n";
print "$output_file: \$(heimdalsrcdir)/$file \$(ASN1C)\n";
print "\t\@echo \"Compiling ASN1 file \$(heimdalsrcdir)/$file\"\n";
print "\t\@\$(heimdalbuildsrcdir)/asn1_compile_wrapper.sh \$(builddir) $dirname \$(ASN1C) \$(call abspath,\$(heimdalsrcdir)/$file) $prefix $options --one-code-file && touch $output_file\n";
print "$headerx: $output_file\n";
print "$header: $headerx\n";
print "\t\@cp $headerx $header\n";
print "$headerpriv: $headerprivx\n";
print "\t\@cp $headerprivx $headerpriv\n";
print "$x_file: $header $headerpriv\n";
print "$c_file: $x_file\n";
print "\t\@echo \"#include \\\"config.h\\\"\" > $c_file && cat $x_file >> $c_file\n\n";

open(IN,"heimdal/$file") or die("Can't open heimdal/$file: $!");
my @lines = <IN>;
close(IN);
foreach my $line (@lines) {
	if ($line =~ /^(\s*IMPORT)([\w\,\s])*(\s+FROM\s+)([\w]+[\w\-]+);/) {
		$import = $line;
		chomp $import;
		push @imports, $import;
		$import = undef;
	} elsif ($line =~ /^(\s*IMPORT).*/) {
		$import = $line;
		chomp $import;
	} elsif (defined($import) and ($line =~ /;/)) {
		$import .= $line;
		chomp $import;
		push @imports, $import;
		$import = undef;
	} elsif (defined($import)) {
		$import .= $line;
		chomp $import;
	}
}

foreach $import (@imports) {
	next unless ($import =~ /^(\s*IMPORT)([\w\,\s])*(\s+FROM\s+)([\w]+[\w\-]+);/);

	my @froms = split (/\s+FROM\s+/, $import);
	foreach my $from (@froms) {
		next if ($from =~ /^(\s*IMPORT).*/);
		if ($from =~ /^(\w+)/) {
			my $f = $1;
			$dep = 'HEIMDAL_'.uc($f).'_ASN1';
			push @deps, $dep;
		}
	}
}

unshift @deps, "HEIMDAL_HEIM_ASN1" unless grep /HEIMDAL_HEIM_ASN1/, @deps;
my $depstr = join(' ', @deps);

print '[SUBSYSTEM::HEIMDAL_'.uc($prefix).']'."\n";
print "CFLAGS = -Iheimdal_build -Iheimdal/lib/roken -I$dirname\n";
print "PUBLIC_DEPENDENCIES = $depstr\n\n";

print "HEIMDAL_".uc($prefix)."_OBJ_FILES = ";
print "\\\n\t$o_file";

print "\n\n";

print "clean:: \n";
print "\t\@echo \"Deleting ASN1 output files generated from $file\"\n";
print "\t\@rm -f $output_file\n";
print "\t\@rm -f $header\n";
print "\t\@rm -f $c_file\n";
print "\t\@rm -f $x_file\n";
print "\t\@rm -f $dirname/$prefix\_files\n";
print "\t\@rm -f $dirname/$prefix\.h\n";
print "\n";
