#!/usr/bin/perl 
#
# Walk through source, add labels and make classes
#
#use strict;

my %deplist;

#open class file and write preamble 
open(CLASS, ">tommath_class.h") or die "Couldn't open tommath_class.h for writing\n";
print CLASS "#if !(defined(LTM1) && defined(LTM2) && defined(LTM3))\n#if defined(LTM2)\n#define LTM3\n#endif\n#if defined(LTM1)\n#define LTM2\n#endif\n#define LTM1\n\n#if defined(LTM_ALL)\n";

foreach my $filename (glob "bn*.c") {
   my $define = $filename;

print "Processing $filename\n";

   # convert filename to upper case so we can use it as a define 
   $define =~ tr/[a-z]/[A-Z]/;
   $define =~ tr/\./_/;
   print CLASS "#define $define\n";

   # now copy text and apply #ifdef as required 
   my $apply = 0;
   open(SRC, "<$filename");
   open(OUT, ">tmp");

   # first line will be the #ifdef
   my $line = <SRC>;
   if ($line =~ /include/) {
      print OUT $line;
   } else {
      print OUT "#include <tommath.h>\n#ifdef $define\n$line";
      $apply = 1;
   }
   while (<SRC>) {
      if (!($_ =~ /tommath\.h/)) {
         print OUT $_;
      }
   }
   if ($apply == 1) {
      print OUT "#endif\n";
   }
   close SRC;
   close OUT;

   unlink($filename);
   rename("tmp", $filename);
}
print CLASS "#endif\n\n";

# now do classes 

foreach my $filename (glob "bn*.c") {
   open(SRC, "<$filename") or die "Can't open source file!\n"; 

   # convert filename to upper case so we can use it as a define 
   $filename =~ tr/[a-z]/[A-Z]/;
   $filename =~ tr/\./_/;

   print CLASS "#if defined($filename)\n";
   my $list = $filename;

   # scan for mp_* and make classes
   while (<SRC>) {
      my $line = $_;
      while ($line =~ m/(fast_)*(s_)*mp\_[a-z_0-9]*/) {
          $line = $';
          # now $& is the match, we want to skip over LTM keywords like
          # mp_int, mp_word, mp_digit
          if (!($& eq "mp_digit") && !($& eq "mp_word") && !($& eq "mp_int")) {
             my $a = $&;
             $a =~ tr/[a-z]/[A-Z]/;
             $a = "BN_" . $a . "_C";
             if (!($list =~ /$a/)) {
                print CLASS "   #define $a\n";
             }
             $list = $list . "," . $a;
          }
      }
   }
   @deplist{$filename} = $list;

   print CLASS "#endif\n\n";
   close SRC;
}

print CLASS "#ifdef LTM3\n#define LTM_LAST\n#endif\n#include <tommath_superclass.h>\n#include <tommath_class.h>\n#else\n#define LTM_LAST\n#endif\n";
close CLASS;

#now let's make a cool call graph... 

open(OUT,">callgraph.txt");
$indent = 0;
foreach (keys %deplist) {
   $list = "";
   draw_func(@deplist{$_});
   print OUT "\n\n";
}
close(OUT);

sub draw_func()
{
   my @funcs = split(",", $_[0]);
   if ($list =~ /@funcs[0]/) {
      return;
   } else {
      $list = $list . @funcs[0];
   }
   if ($indent == 0) { }
   elsif ($indent >= 1) { print OUT "|   " x ($indent - 1) . "+--->"; }
   print OUT @funcs[0] . "\n";   
   shift @funcs;
      my $temp = $list;
   foreach my $i (@funcs) {
      ++$indent;
      draw_func(@deplist{$i});
      --$indent;
   }
      $list = $temp;
}


