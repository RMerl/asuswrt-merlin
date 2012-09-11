#!/bin/sh

RRDTOOL=/usr/bin/rrdtool
OUTDIR=/var/www/servers/www.example.org/pages/rrd/
INFILE=/var/www/lighttpd.rrd
OUTPRE=lighttpd-traffic

DISP="DEF:bin=$INFILE:InOctets:AVERAGE \
      DEF:binmin=$INFILE:InOctets:MIN \
      DEF:binmax=$INFILE:InOctets:MAX \
      DEF:bout=$INFILE:OutOctets:AVERAGE \
      DEF:boutmin=$INFILE:OutOctets:MIN \
      DEF:boutmax=$INFILE:OutOctets:MAX \
      LINE1:bin#0000FF:in \
      LINE1:binmin#2222FF: \
      STACK:binmax#2222FF: \
      LINE1:bout#FF0000:out \
      LINE1:boutmin#FF2222: \
      STACK:boutmax#FF2222: \
      -v bytes/s"

$RRDTOOL graph $OUTDIR/$OUTPRE-hour.png -a PNG --start -14400 $DISP
$RRDTOOL graph $OUTDIR/$OUTPRE-day.png -a PNG --start -86400 $DISP
$RRDTOOL graph $OUTDIR/$OUTPRE-month.png -a PNG --start -2592000 $DISP

OUTPRE=lighttpd-requests

DISP="DEF:req=$INFILE:Requests:AVERAGE \
      DEF:reqmin=$INFILE:Requests:MIN \
      DEF:reqmax=$INFILE:Requests:MAX \
      LINE1:req#0000FF:requests \
      LINE1:reqmin#2222FF: \
      STACK:reqmax#2222FF: \
      -v req/s"

$RRDTOOL graph $OUTDIR/$OUTPRE-hour.png -a PNG --start -14400 $DISP
$RRDTOOL graph $OUTDIR/$OUTPRE-day.png -a PNG --start -86400 $DISP
$RRDTOOL graph $OUTDIR/$OUTPRE-month.png -a PNG --start -2592000 $DISP
