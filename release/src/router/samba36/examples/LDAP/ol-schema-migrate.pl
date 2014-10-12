#!/usr/bin/perl -w
#
# Convert OpenLDAP schema files into Fedora DS format with RFC2252 compliant printing
#
# First Release : Mike Jackson <mj@sci.fi> 14 June 2005
#    http://www.netauth.com/~jacksonm/ldap/ol-schema-migrate.pl
#    Professional LDAP consulting for large and small projects
#
# - 6 Dec 2005
# - objectclass element ordering
#
# Second Release : Alyseo <info@alyseo.com> 05 Februrary 2006
#    Francois Billard <francois@alyseo.com>
#    Yacine Kheddache <yacine@alyseo.com>
#    http://www.alyseo.com/
#
# - 05 Februrary 2006
#  - parsing improvement to accept non-RFC compliant schemas (like ISPMAN)
#  - adding RFC element : Usage, No-user-modification, collective keywords
# - 08 Februrary 2006
#  - adding help & usage
#  - now this script can also beautify your schemas: "-b"
#  - count attributes and objects class: "-c"
#  - display items that can not be converted (empty OID...): "-d"
# - 15 February 2006
#  - adding workaround for Fedora DS bug 181465:
#    https://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=181465
#  - adding duplicated OID check: "-d"
#    Useful to manually correct nasty schemas like:
#    https://sourceforge.net/tracker/?func=detail&atid=108390&aid=1429276&group_id=8390
# - 13 September 2007
#    Based on Samba Team GPL Compliance Officer request, license has been updated from 
#    GPL to GPLv3+
#
# - Fedora DS bug you need to correct by hand :
#    https://bugzilla.redhat.com/bugzilla/show_bug.cgi?id=179956
#
# GPLv3+ license
#

my $optionCount = 0;
my $optionPrint = 0;
my $optionBadEntries = 0;
my $optionHelp = 0;
my $filename = "" ;

foreach (@ARGV) {
  $optionHelp = 1 if ( /^-h$/);
  $optionCount = 1 if ( /^-c$/);
  $optionPrint = 1 if ( /^-b$/);
  $optionBadEntries = 1 if ( /^-d$/);
  $filename = $_ if ( ! /^-b$/ && ! /^-c$/ && ! /^-d$/);
}

die "Usage : ol-schema-migrate-v2.pl [ -c ] [ -b ] [ -d ] schema\n" . 
    "  -c\tcount attribute and object class\n" . 
    "  -b\tconvert and beautify your schema\n" .
    "  -d\tdisplay unrecognized elements, find empty and duplicated OID\n" .
    "  -h\tthis help\n" if ($filename eq "" || ($optionHelp || (!$optionCount && !$optionPrint && !$optionBadEntries)));

if($optionCount) {
  print "Schema verification counters:\n";
  my $ldapdata = &getSourceFile($filename);
  print "".(defined($ldapdata->{attributes}) ? @{$ldapdata->{attributes}} : 0) . " attributes\n";
  print "".(defined($ldapdata->{objectclass}) ?  @{$ldapdata->{objectclass}} : 0) . " object classes\n\n" 
}

if($optionPrint) {
  my $ldapdata = &getSourceFile($filename);
  &printit($ldapdata);
}

if($optionBadEntries) {
  print "Display unrecognized entries:\n";
  my $ldapdata = &getSourceFile($filename);
  my $errorsAttr = 0;
  my $errorsObjc = 0;
  my $errorsDup  = 0;
  my $emptyOid = 0;
  my %dup;
  
  foreach (@{$ldapdata->{attributes}}) {
    my $attr = $_;
    
    push @{$dup{$attr->{OID}}{attr}}, {NAME => $attr->{NAME}, LINENUMBER => $attr->{LINENUMBER}};
    
    $attr->{DATA} =~ s/\n/ /g;
    $attr->{DATA} =~ s/\r//g;
    $attr->{DATA} =~ s/attribute[t|T]ypes?:?\s*\(//;
    $attr->{DATA} =~ s/\Q$attr->{OID}//                 if(defined $attr->{OID});
    $attr->{DATA} =~ s/NAME\s*\Q$attr->{NAME}//         if(defined $attr->{NAME});
    $attr->{DATA} =~ s/DESC\s*'\Q$attr->{DESC}'//       if(defined $attr->{DESC});
    $attr->{DATA} =~ s/$attr->{OBSOLETE}//              if(defined $attr->{OBSOLETE});
    $attr->{DATA} =~ s/SUP\s*\Q$attr->{SUP}//           if(defined $attr->{SUP});
    $attr->{DATA} =~ s/EQUALITY\s*\Q$attr->{EQUALITY}// if(defined $attr->{EQUALITY});
    $attr->{DATA} =~ s/ORDERING\s*\Q$attr->{ORDERING}// if(defined $attr->{ORDERING});
    $attr->{DATA} =~ s/SUBSTR\s*\Q$attr->{SUBSTR}//     if(defined $attr->{SUBSTR});
    $attr->{DATA} =~ s/SYNTAX\s*\Q$attr->{SYNTAX}//     if(defined $attr->{SYNTAX});
    $attr->{DATA} =~ s/SINGLE-VALUE//                   if(defined $attr->{SINGLEVALUE});
    $attr->{DATA} =~ s/NO-USER-MODIFICATION//           if(defined $attr->{NOUSERMOD});
    $attr->{DATA} =~ s/COLLECTIVE//                     if(defined $attr->{COLLECTIVE});
    $attr->{DATA} =~ s/USAGE\s*\Q$attr->{USAGE}//       if(defined $attr->{USAGE});
    $attr->{DATA} =~ s/\)\s$//;
    $attr->{DATA} =~ s/^\s+(\S)/\n$1/  ;
    $attr->{DATA} =~ s/(\S)\s+$/$1\n/;
    do {
      $errorsAttr ++;      
      do {  $emptyOid ++; 
            print "Warning : no OID for attributes element at line $attr->{LINENUMBER} \n";
      } if( !defined($attr->{OID}));
      print "### Unknow element embedded in ATTRIBUTE at line $attr->{LINENUMBER} :\n$attr->{DATA}\n"
    } if($attr->{DATA} =~ /\w/);
  }

  foreach (@{$ldapdata->{objectclass}}) {
    my $objc = $_;
    push @{$dup{$objc->{OID}}{objc}} , {NAME => $objc->{NAME}, LINENUMBER => $objc->{LINENUMBER}};
    $objc->{DATA} =~ s/\n/ /g;
    $objc->{DATA} =~ s/\r//g;
    $objc->{DATA} =~ s/^object[c|C]lasse?s?:?\s*\(?//;
    $objc->{DATA} =~ s/\Q$objc->{OID}//                       if(defined $objc->{OID});
    $objc->{DATA} =~ s/NAME\s*\Q$objc->{NAME}\E//             if(defined $objc->{NAME});
    $objc->{DATA} =~ s/DESC\s*'\Q$objc->{DESC}\E'//           if(defined $objc->{DESC});
    $objc->{DATA} =~ s/OBSOLETE//                             if(defined $objc->{OBSOLETE});
    $objc->{DATA} =~ s/SUP\s*\Q$objc->{SUP}//                 if(defined $objc->{SUP});
    $objc->{DATA} =~ s/\Q$objc->{TYPE}//                      if(defined $objc->{TYPE});
    $objc->{DATA} =~ s/MUST\s*\Q$objc->{MUST}\E\s*//          if(defined $objc->{MUST});
    $objc->{DATA} =~ s/MUST\s*\(?\s*\Q$objc->{MUST}\E\s*\)?// if(defined $objc->{MUST});
    $objc->{DATA} =~ s/MAY\s*\Q$objc->{MAY}\E//               if(defined $objc->{MAY});
    $objc->{DATA} =~ s/\)\s$//;
    $objc->{DATA} =~ s/^\s+(\S)/\n$1/  ;
    $objc->{DATA} =~ s/(\S)\s+$/$1\n/;

    do {
      print "#" x 80 ."\n";
      $errorsObjc ++;
      do { $emptyOid++ ;
           print "Warning : no OID for object class element at line $objc->{LINENUMBER} \n";
      } if( $objc->{OID} eq "");
      print "### Unknow element embedded in OBJECT CLASS at line $objc->{LINENUMBER} :\n$objc->{DATA}\n"
    } if($objc->{DATA} =~ /\w/);
  }

  my $nbDup = 0;
  foreach (keys %dup) {
    my $sumOid = 0;
    $sumOid += @{$dup{$_}{attr}} if(defined (@{$dup{$_}{attr}}));
    $sumOid += @{$dup{$_}{objc}} if(defined (@{$dup{$_}{objc}}));
    if( $sumOid > 1 && $_ ne "") {
      $nbDup ++;
      print "#" x 80 ."\n";
      print "Duplicate OID founds : $_\n";
      foreach (@{$dup{$_}{attr}}) {
        
        print "Attribute : $_->{NAME} (line : $_->{LINENUMBER})\n";
      }
      foreach (@{$dup{$_}{objc}}) {
        print "Object class : $_->{NAME} (line : $_->{LINENUMBER})\n";
      }
      
    }
  }

  print "\n$errorsAttr errors detected in ATTRIBUTES list\n";
  print "$errorsObjc errors detected in OBJECT CLASS list\n";
  print "$nbDup duplicate OID founds\n";
  print "$emptyOid empty OID fields founds\n\n";

}


sub printit {
  my $ldapdata = shift;
  &printSeparator;
  print "dn: cn=schema\n";
  &printSeparator;

  # print elements in RFC2252 order

  foreach (@{$ldapdata->{attributes}}) {
    my $attr = $_;
    print "attributeTypes: (\n";
    print "  $attr->{OID}\n";
    print "  NAME $attr->{NAME}\n";
    print "  DESC '$attr->{DESC}'\n"         if(defined $attr->{DESC});
    print "  OBSOLETE\n"                     if(defined $attr->{OBSOLETE});
    print "  SUP $attr->{SUP}\n"             if(defined $attr->{SUP});
    print "  EQUALITY $attr->{EQUALITY}\n"   if(defined $attr->{EQUALITY});
    print "  ORDERING $attr->{ORDERING}\n"   if(defined $attr->{ORDERING});
    print "  SUBSTR $attr->{SUBSTR}\n"       if(defined $attr->{SUBSTR});
    print "  SYNTAX $attr->{SYNTAX}\n"       if(defined $attr->{SYNTAX});
    print "  SINGLE-VALUE\n"                 if(defined $attr->{SINGLEVALUE});
    print "  NO-USER-MODIFICATION\n"         if(defined $attr->{NOUSERMOD});
    print "  COLLECTIVE\n"                   if(defined $attr->{COLLECTIVE});
    print "  USAGE $attr->{USAGE}\n"         if(defined $attr->{USAGE});
    print "  )\n";
    &printSeparator;
  }

  foreach (@{$ldapdata->{objectclass}}) {
    my $objc = $_;
    # next 3 lines : Fedora DS space sensitive bug workaround 
    $objc->{SUP}         =~ s/^\(\s*(.*?)\s*\)$/\( $1 \)/  if (defined $objc->{SUP});  
    $objc->{MUST}        =~ s/^\(\s*(.*?)\s*\)$/\( $1 \)/  if (defined $objc->{MUST}); 
    $objc->{MAY}         =~ s/^\(\s*(.*?)\s*\)$/\( $1 \)/  if (defined $objc->{MAY}); 

    print "objectClasses: (\n";
    print "  $objc->{OID}\n";
    print "  NAME $objc->{NAME}\n";
    print "  DESC '$objc->{DESC}'\n"  if(defined $objc->{DESC});
    print "  OBSOLETE\n"              if(defined $objc->{OBSOLETE});
    print "  SUP $objc->{SUP}\n"      if(defined $objc->{SUP});
    print "  $objc->{TYPE}\n"         if(defined $objc->{TYPE});  
    print "  MUST $objc->{MUST}\n"    if(defined $objc->{MUST});
    print "  MAY $objc->{MAY}\n"      if(defined $objc->{MAY});
    print "  )\n";
    &printSeparator;
  }
}

sub printSeparator {
  print "#\n";
  print "#" x 80 . "\n";
  print "#\n";
}  

sub getSourceFile {
  my @data = &getFile(shift);
  my %result;
  my $result = \%result;
  my @allattrs;
  my @allattrsLineNumber;
  my @allobjc;
  my @allobjcLineNumber;
  my $at = 0;
  my $oc = 0;
  my $at_string;
  my $oc_string;
  my $idx = 0;
  my $beginParenthesis = 0;
  my $endParenthesis = 0;
  my $lineNumber = 0;  
  for(@data) {
    $lineNumber++;
    next if (/^\s*\#/); # skip comments

    if($at) {
      s/ +/ /;                    # remove embedded tabs
      s/\t/ /;                    # remove multiple spaces after the $ sign

       $at_string .= $_;
      $beginParenthesis = 0;      # Use best matching elements
      $endParenthesis = 0;
      for(my $i=0;$ i < length($at_string); $i++) {
        $beginParenthesis++ if(substr ($at_string,$i,1) eq "(");
        $endParenthesis++ if(substr ($at_string,$i,1) eq ")");
      }
      if($beginParenthesis == $endParenthesis) {
        push @allattrs, $at_string;
        $at = 0;
        $at_string = "";
        $endParenthesis = 0;
        $beginParenthesis = 0;
      }
    }

    if (/^attribute[t|T]ype/) {
      my $line = $_;
       push @allattrsLineNumber, $lineNumber;      # keep starting line number
      for(my $i=0;$ i < length($line); $i++) {
        $beginParenthesis++ if(substr ($line, $i, 1) eq "(");
        $endParenthesis++ if(substr ($line, $i, 1) eq ")");
      }
      if($beginParenthesis == $endParenthesis && $beginParenthesis != 0) {
        push @allattrs, $line;
        $endParenthesis = 0;
        $beginParenthesis = 0;
      } else {
        $at_string = $line;
        $at = 1;
      }
    }

    #####################################

    if($oc) {
      s/ +/ /;
      s/\t/ /;
      
      $oc_string .= $_;
      $endParenthesis = 0;          # best methode to accept an elements : 
      $beginParenthesis = 0;        # left parenthesis sum == right parenthesis sum, so we are sure to 
      for(my $i=0;$ i < length($oc_string); $i++) {      # have an element.
        $beginParenthesis++ if(substr ($oc_string, $i, 1) eq "(");
        $endParenthesis++ if(substr ($oc_string, $i, 1) eq ")");
      }
      if($beginParenthesis == $endParenthesis) {
        push @allobjc, $oc_string;
        $oc = 0;
        $oc_string = "";
        $endParenthesis = 0;
        $beginParenthesis = 0;
      }
    }

    if (/^object[c|C]lass/) {
      my $line = $_;
       push @allobjcLineNumber, $lineNumber;    # keep starting line number
      for(my $i=0;$ i < length($line); $i++) {
        $beginParenthesis++ if(substr ($line, $i, 1) eq "(");
        $endParenthesis++ if(substr ($line, $i, 1) eq ")");
      }
      if($beginParenthesis == $endParenthesis && $beginParenthesis != 0) {
        push @allobjc, $line;
        $endParenthesis = 0;
        $beginParenthesis = 0;
      } else {
        $oc_string = $line;
        $oc = 1;
      }
    }
  }

  # Parsing attribute elements
  
  for(@allattrs) {
    s/\n/ /g;
    s/\r//g;
    s/ +/ /g;
    s/\t/ /g;
    $result->{attributes}->[$idx]->{DATA}        = $_              if($optionBadEntries);     # keep original data
    $result->{attributes}->[$idx]->{LINENUMBER}  = $allattrsLineNumber[$idx];
    $result->{attributes}->[$idx]->{OID}         = $1              if (m/^attribute[t|T]ypes?:?\s*\(?\s*([\.\d]*?)\s+/);
    $result->{attributes}->[$idx]->{NAME}        = $1              if (m/NAME\s+('.*?')\s*/ || m/NAME\s+(\(.*?\))/);
    $result->{attributes}->[$idx]->{DESC}        = $1              if (m/DESC\s+'(.*?)'\s*/);
    $result->{attributes}->[$idx]->{OBSOLETE}    = "OBSOLETE"      if (m/OBSOLETE/);
    $result->{attributes}->[$idx]->{SUP}         = $1              if (m/SUP\s+(.*?)\s/);
    $result->{attributes}->[$idx]->{EQUALITY}    = $1              if (m/EQUALITY\s+(.*?)\s/);
    $result->{attributes}->[$idx]->{ORDERING}    = $1              if (m/ORDERING\s+(.*?)\s/);
    $result->{attributes}->[$idx]->{SUBSTR}      = $1              if (m/SUBSTR\s+(.*?)\s/);
    $result->{attributes}->[$idx]->{SYNTAX}      = $1              if (m/SYNTAX\s+(.*?)(\s|\))/);
    $result->{attributes}->[$idx]->{SINGLEVALUE} = "SINGLE-VALUE"  if (m/SINGLE-VALUE/);
    $result->{attributes}->[$idx]->{COLLECTIVE}  = "COLLECTIVE"    if (m/COLLECTIVE/);
    $result->{attributes}->[$idx]->{USAGE}       = $1              if (m/USAGE\s+(.*?)\s/);
    $result->{attributes}->[$idx]->{NOUSERMOD}   = "NO-USER-MODIFICATION"    if (m/NO-USER-MODIFICATION/);
    $idx ++;
  }
  
  $idx = 0;
  
  # Parsing object class elements
  
  for(@allobjc) {
    s/\n/ /g;
    s/\r//g;
    s/ +/ /g;
    s/\t/ /g;
    $result->{objectclass}->[$idx]->{DATA}        = $_          if($optionBadEntries);     # keep original data
    $result->{objectclass}->[$idx]->{LINENUMBER}  = $allobjcLineNumber[$idx];
    $result->{objectclass}->[$idx]->{OID}         = $1          if (m/^object[c|C]lasse?s?:?\s*\(?\s*([\.\d]*?)\s+/);
    $result->{objectclass}->[$idx]->{NAME}        = $1          if (m/NAME\s+('.*?')\s*/ || m/NAME\s+(\(.*?\))/);
    $result->{objectclass}->[$idx]->{DESC}        =  $1         if (m/DESC\s+'(.*?)'\s*/);
    $result->{objectclass}->[$idx]->{OBSOLETE}    = "OBSOLETE"  if (m/OBSOLETE/);
    $result->{objectclass}->[$idx]->{SUP}         = $1          if (m/SUP\s+([^()]+?)\s/ || m/SUP\s+(\(.+?\))\s/);
    $result->{objectclass}->[$idx]->{TYPE}        = $1          if (m/((?:STRUCTURAL)|(?:AUXILIARY)|(?:ABSTRACT))/);
    $result->{objectclass}->[$idx]->{MUST}        = $1          if (m/MUST\s+(\w+)\)?/ || m/MUST\s+(\(.*?\))(\s|\))/s);
    $result->{objectclass}->[$idx]->{MAY}         = $1          if (m/MAY\s+(\w+)\)?/ || m/MAY\s+(\(.*?\))(\s|\))/s);

    $idx++;
  }
  
  return $result;
}

sub getFile {
  my @data;
  my $file = shift;
  die "File not found : $file\n" if(! -e $file);
  open FH, $file;
  @data = <FH>;
  close FH;
  @data;
}

