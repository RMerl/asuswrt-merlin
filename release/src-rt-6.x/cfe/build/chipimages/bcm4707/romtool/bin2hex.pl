#! /usr/bin/perl -w

   ($in, $out) = @ARGV;
   die "Missing input file name.\n" unless $in;
   die "Missing output file name.\n" unless $out;
   $byteCount = 0;
   open(IN, "< $in");
   binmode(IN);
   open(OUT, "> tmp.hex");
   while (read(IN,$b,1)) {
      $n = length($b);
      $byteCount += $n;
      $s = 2*$n;
      print (OUT unpack("H$s", $b), "\n");
      
   }
   close(IN);
   close(OUT);
   print "Number of bytes converted = $byteCount\n";


   open(IN, "< tmp.hex");
   open(OUT, "> $out");
   $addr = 0x0;
     printf OUT ("\$INSTANCE qt_serial_flash.mem\n");
     printf OUT ("\$RADIX   HEX\n");
     printf OUT ("\$ADDRESS   0    7ff\n");
   while (<IN>) {
     printf OUT ("%x $_",$addr);
     $addr++;
   }

system("rm tmp.hex");
