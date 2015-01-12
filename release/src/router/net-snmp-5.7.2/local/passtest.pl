#!/usr/bin/perl
$place = ".1.3.6.1.4.1.8072.2.255";   # NET-SNMP-PASS-MIB::netSnmpPassExamples
$req = $ARGV[1];                      # Requested OID

#
#  Process SET requests by simply logging the assigned value
#      Note that such "assignments" are not persistent,
#      nor is the syntax or requested value validated
#  
if ($ARGV[0] eq "-s") {
  open  LOG,">>/tmp/passtest.log";
  print LOG "@ARGV\n";
  close LOG;
  exit 0;
}

#
#  GETNEXT requests - determine next valid instance
#
if ($ARGV[0] eq "-n") {
     if (($req eq  "$place")         ||
         ($req eq  "$place.0")       ||
         ($req =~ m/$place\.0\..*/)  ||
         ($req eq  "$place.1"))       { $ret = "$place.1.0";}       # netSnmpPassString.0
  elsif (($req =~ m/$place\.1\..*/)  ||
         ($req eq  "$place.2")       ||
         ($req eq  "$place.2.0")     ||
         ($req =~ m/$place\.2\.0\..*/)    ||
         ($req eq  "$place.2.1")          ||
         ($req eq  "$place.2.1.0")        ||
         ($req =~ m/$place\.2\.1\.0\..*/) ||
         ($req eq  "$place.2.1.1")        ||
         ($req =~ m/$place\.2\.1\.1\..*/) ||
         ($req eq  "$place.2.1.2")        ||
         ($req eq  "$place.2.1.2.0")) { $ret = "$place.2.1.2.1";}   # netSnmpPassInteger.1
  elsif (($req =~ m/$place\.2\.1\.2\..*/) ||
         ($req eq  "$place.2.1.3")   ||
         ($req eq  "$place.2.1.3.0")) { $ret = "$place.2.1.3.1";}   # netSnmpPassOID.1
  elsif (($req =~ m/$place\.2\..*/)  ||
         ($req eq  "$place.3"))       { $ret = "$place.3.0";}       # netSnmpPassTimeTicks.0
  elsif (($req =~ m/$place\.3\..*/)  ||
         ($req eq  "$place.4"))       { $ret = "$place.4.0";}       # netSnmpPassIpAddress.0
  elsif (($req =~ m/$place\.4\..*/)  ||
         ($req eq  "$place.5"))       { $ret = "$place.5.0";}       # netSnmpPassCounter.0
  elsif (($req =~ m/$place\.5\..*/)  ||
         ($req eq  "$place.6"))       { $ret = "$place.6.0";}       # netSnmpPassGauge.0
  else   {exit 0;}
}
else {
#
#  GET requests - check for valid instance
#
  if ( ($req eq "$place.1.0")     ||
       ($req eq "$place.2.1.2.1") ||
       ($req eq "$place.2.1.3.1") ||
       ($req eq "$place.3.0")     ||
       ($req eq "$place.3.0")     ||
       ($req eq "$place.3.0")     ||
       ($req eq "$place.3.0"))     { $ret = $req; }
  else { exit 0;}
}

#
#  "Process" GET* requests - return hard-coded value
#
print "$ret\n";
   if ($ret eq "$place.1.0")     { print "string\nLife, the Universe, and Everything\n"; exit 0;}
elsif ($ret eq "$place.2.1.2.1") { print "integer\n42\n";                                exit 0;}
elsif ($ret eq "$place.2.1.3.1") { print "objectid\n$place.99\n";                        exit 0;}
elsif ($ret eq "$place.3.0")     { print "timeticks\n363136200\n";                       exit 0;}
elsif ($ret eq "$place.4.0")     { print "ipaddress\n127.0.0.1\n";                       exit 0;}
elsif ($ret eq "$place.5.0")     { print "counter\n42\n";                                exit 0;}
elsif ($ret eq "$place.6.0")     { print "gauge\n42\n";                                  exit 0;}
else                             { print "string\nack... $ret $req\n";                   exit 0;}  # Should not happen
