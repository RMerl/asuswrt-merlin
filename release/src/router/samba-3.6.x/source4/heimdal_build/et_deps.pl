#!/usr/bin/perl

use File::Basename;

my $file = shift;
my $dirname = shift;
my $basename = basename($file);

my $header = "$dirname/$basename"; $header =~ s/\.et$/.h/;
my $source = "$dirname/$basename"; $source =~ s/\.et$/.c/;
print "basics:: $header\n";
print "$header $source: \$(heimdalsrcdir)/$file \$(ET_COMPILER)\n";
print "\t\@echo \"Compiling error table $file\"\n";
print "\t\@\$(heimdalbuildsrcdir)/et_compile_wrapper.sh \$(builddir) $dirname \$(ET_COMPILER) \$(call abspath,\$(heimdalsrcdir)/$file) $source\n\n";

print "clean:: \n";
print "\t\@rm -f $header $source\n\n";
