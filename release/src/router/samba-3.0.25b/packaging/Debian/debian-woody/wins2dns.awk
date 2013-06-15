#!/usr/bin/awk -f
#
# Date: Wed, 26 Aug 1998 10:37:39 -0600 (MDT)
# From: Jason Gunthorpe <jgg@deltatee.com>
# To: samba@packages.debian.org
# Subject: Nifty samba script
#
# Here is a really nifty script I just wrote for samba, it takes the wins
# database in /var/samba/wins and writes out two dns files for it. In this
# way network wide wins clients can get into the dns for use by unix
# machines.
# 
# Perhaps this could be included in  /usr/doc/examples or somesuch.
#

BEGIN {
  FS="#|\"";
FORWARD="/tmp/wins.hosts"
REVERSE="/tmp/wins.rev"
DOMAIN="ven.ra.rockwell.com"
}
$3 == "00" {
  split($4,a," " );
  split(a[2],b,".");
  while (sub(" ","-",$2));
  $2=tolower($2);
  if (b[1] == "255")
    next;
  if (length($2) >= 8)
    print $2"\ta\t"a[2] > FORWARD
  else
    print $2"\t\ta\t"a[2] > FORWARD
  print b[4]"."b[3]"\t\tptr\t"$2"."DOMAIN"." > REVERSE
}
END {
  system("echo killall -HUP named");
}

