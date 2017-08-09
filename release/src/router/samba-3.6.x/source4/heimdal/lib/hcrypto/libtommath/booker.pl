#!/bin/perl
#
#Used to prepare the book "tommath.src" for LaTeX by pre-processing it into a .tex file
#
#Essentially you write the "tommath.src" as normal LaTex except where you want code snippets you put
#
#EXAM,file
#
#This preprocessor will then open "file" and insert it as a verbatim copy.
#
#Tom St Denis

#get graphics type
if (shift =~ /PDF/) {
   $graph = "";
} else {
   $graph = ".ps";
}   

open(IN,"<tommath.src") or die "Can't open source file";
open(OUT,">tommath.tex") or die "Can't open destination file";

print "Scanning for sections\n";
$chapter = $section = $subsection = 0;
$x = 0;
while (<IN>) {
   print ".";
   if (!(++$x % 80)) { print "\n"; }
   #update the headings 
   if (~($_ =~ /\*/)) {
      if ($_ =~ /\\chapter{.+}/) {
          ++$chapter;
          $section = $subsection = 0;
      } elsif ($_ =~ /\\section{.+}/) {
          ++$section;
          $subsection = 0;
      } elsif ($_ =~ /\\subsection{.+}/) {
          ++$subsection;
      }
   }      

   if ($_ =~ m/MARK/) {
      @m = split(",",$_);
      chomp(@m[1]);
      $index1{@m[1]} = $chapter;
      $index2{@m[1]} = $section;
      $index3{@m[1]} = $subsection;
   }
}
close(IN);

open(IN,"<tommath.src") or die "Can't open source file";
$readline = $wroteline = 0;
$srcline = 0;

while (<IN>) {
   ++$readline;
   ++$srcline;
   
   if ($_ =~ m/MARK/) {
   } elsif ($_ =~ m/EXAM/ || $_ =~ m/LIST/) {
      if ($_ =~ m/EXAM/) {
         $skipheader = 1;
      } else {
         $skipheader = 0;
      }
      
      # EXAM,file
      chomp($_);
      @m = split(",",$_);
      open(SRC,"<$m[1]") or die "Error:$srcline:Can't open source file $m[1]";
      
      print "$srcline:Inserting $m[1]:";
      
      $line = 0;
      $tmp = $m[1];
      $tmp =~ s/_/"\\_"/ge;
      print OUT "\\vspace{+3mm}\\begin{small}\n\\hspace{-5.1mm}{\\bf File}: $tmp\n\\vspace{-3mm}\n\\begin{alltt}\n";
      $wroteline += 5;
      
      if ($skipheader == 1) {
         # scan till next end of comment, e.g. skip license 
         while (<SRC>) {
            $text[$line++] = $_;
            last if ($_ =~ /math\.libtomcrypt\.com/);
         }
         <SRC>;   
      }
      
      $inline = 0;
      while (<SRC>) {
      next if ($_ =~ /\$Source/);
      next if ($_ =~ /\$Revision/);
      next if ($_ =~ /\$Date/);
         $text[$line++] = $_;
         ++$inline;
         chomp($_);
         $_ =~ s/\t/"    "/ge;
         $_ =~ s/{/"^{"/ge;
         $_ =~ s/}/"^}"/ge;
         $_ =~ s/\\/'\symbol{92}'/ge;
         $_ =~ s/\^/"\\"/ge;
           
         printf OUT ("%03d   ", $line);
         for ($x = 0; $x < length($_); $x++) {
             print OUT chr(vec($_, $x, 8));
             if ($x == 75) { 
                 print OUT "\n      ";
                 ++$wroteline;
             }
         }
         print OUT "\n";
         ++$wroteline;
      }
      $totlines = $line;
      print OUT "\\end{alltt}\n\\end{small}\n";
      close(SRC);
      print "$inline lines\n";
      $wroteline += 2;
   } elsif ($_ =~ m/@\d+,.+@/) {
     # line contains [number,text]
     # e.g. @14,for (ix = 0)@
     $txt = $_;
     while ($txt =~ m/@\d+,.+@/) {
        @m = split("@",$txt);      # splits into text, one, two
        @parms = split(",",$m[1]);  # splits one,two into two elements 
                
        # now search from $parms[0] down for $parms[1] 
        $found1 = 0;
        $found2 = 0;
        for ($i = $parms[0]; $i < $totlines && $found1 == 0; $i++) {
           if ($text[$i] =~ m/\Q$parms[1]\E/) {
              $foundline1 = $i + 1;
              $found1 = 1;
           }
        }
        
        # now search backwards
        for ($i = $parms[0] - 1; $i >= 0 && $found2 == 0; $i--) {
           if ($text[$i] =~ m/\Q$parms[1]\E/) {
              $foundline2 = $i + 1;
              $found2 = 1;
           }
        }
        
        # now use the closest match or the first if tied
        if ($found1 == 1 && $found2 == 0) {
           $found = 1;
           $foundline = $foundline1;
        } elsif ($found1 == 0 && $found2 == 1) {
           $found = 1;
           $foundline = $foundline2;
        } elsif ($found1 == 1 && $found2 == 1) {
           $found = 1;
           if (($foundline1 - $parms[0]) <= ($parms[0] - $foundline2)) {
              $foundline = $foundline1;
           } else {
              $foundline = $foundline2;
           }
        } else {
           $found = 0;
        }
                      
        # if found replace 
        if ($found == 1) {
           $delta = $parms[0] - $foundline;
           print "Found replacement tag for \"$parms[1]\" on line $srcline which refers to line $foundline (delta $delta)\n";
           $_ =~ s/@\Q$m[1]\E@/$foundline/;
        } else {
           print "ERROR:  The tag \"$parms[1]\" on line $srcline was not found in the most recently parsed source!\n";
        }
        
        # remake the rest of the line 
        $cnt = @m;
        $txt = "";
        for ($i = 2; $i < $cnt; $i++) {
            $txt = $txt . $m[$i] . "@";
        }
     }
     print OUT $_;
     ++$wroteline;
   } elsif ($_ =~ /~.+~/) {
      # line contains a ~text~ pair used to refer to indexing :-)
      $txt = $_;
      while ($txt =~ /~.+~/) {
         @m = split("~", $txt);
         
         # word is the second position
         $word = @m[1];
         $a = $index1{$word};
         $b = $index2{$word};
         $c = $index3{$word};
         
         # if chapter (a) is zero it wasn't found
         if ($a == 0) {
            print "ERROR: the tag \"$word\" on line $srcline was not found previously marked.\n";
         } else {
            # format the tag as x, x.y or x.y.z depending on the values
            $str = $a;
            $str = $str . ".$b" if ($b != 0);
            $str = $str . ".$c" if ($c != 0);
            
            if ($b == 0 && $c == 0) {
               # its a chapter
               if ($a <= 10) {
                  if ($a == 1) {
                     $str = "chapter one";
                  } elsif ($a == 2) {
                     $str = "chapter two";
                  } elsif ($a == 3) {
                     $str = "chapter three";
                  } elsif ($a == 4) {
                     $str = "chapter four";
                  } elsif ($a == 5) {
                     $str = "chapter five";
                  } elsif ($a == 6) {
                     $str = "chapter six";
                  } elsif ($a == 7) {
                     $str = "chapter seven";
                  } elsif ($a == 8) {
                     $str = "chapter eight";
                  } elsif ($a == 9) {
                     $str = "chapter nine";
                  } elsif ($a == 10) {
                     $str = "chapter ten";
                  }
               } else {
                  $str = "chapter " . $str;
               }
            } else {
               $str = "section " . $str     if ($b != 0 && $c == 0);            
               $str = "sub-section " . $str if ($b != 0 && $c != 0);
            }
            
            #substitute
            $_ =~ s/~\Q$word\E~/$str/;
            
            print "Found replacement tag for marker \"$word\" on line $srcline which refers to $str\n";
         }
         
         # remake rest of the line
         $cnt = @m;
         $txt = "";
         for ($i = 2; $i < $cnt; $i++) {
             $txt = $txt . $m[$i] . "~";
         }
      }
      print OUT $_;
      ++$wroteline;
   } elsif ($_ =~ m/FIGU/) {
      # FIGU,file,caption
      chomp($_);
      @m = split(",", $_);
      print OUT "\\begin{center}\n\\begin{figure}[here]\n\\includegraphics{pics/$m[1]$graph}\n";
      print OUT "\\caption{$m[2]}\n\\label{pic:$m[1]}\n\\end{figure}\n\\end{center}\n";
      $wroteline += 4;
   } else {
      print OUT $_;
      ++$wroteline;
   }
}
print "Read $readline lines, wrote $wroteline lines\n";

close (OUT);
close (IN);
