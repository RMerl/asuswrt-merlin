#!/bin/sh
# test groupmap code tridge@samba.org September 2006
# note that this needs root access to add unix groups,
# so this cannot be run on the build farm

testone() {
    echo $*
    $VALGRIND bin/net groupmap $*
}

tstart() {
    TBASE=`date '+%s'`
}

treport() {
    TNOW=`date '+%s'`
    echo "Took `expr $TNOW - $TBASE` seconds"
    TBASE=$TNOW
}

rm -f $PREFIX_ABS/var/locks/group_mapping.?db

NLOCAL=12
NGROUP=11
NBUILTIN=10
DOMSID=`bin/net getlocalsid | awk '{print $6}'`
FORSID="S-1-2-3-4-5"

echo "DOMSID $DOMSID"
echo "FORSID $FORSID"

tstart
echo "Creating unix groups"
for i in `seq 1 1 $NLOCAL`; do
  unixgroup=testlocal$i;
  gid=`expr 30000 + $i`;
  groupdel $unixgroup 2> /dev/null
  groupadd -g $gid $unixgroup || exit 1
done
for i in `seq 1 1 $NGROUP`; do
  unixgroup=testgrp$i;
  gid=`expr 40000 + $i`;
  groupdel $unixgroup 2> /dev/null
  groupadd -g $gid $unixgroup || exit 1
done
for i in `seq 1 1 $NBUILTIN`; do
  unixgroup=testb$i;
  gid=`expr 50000 + $i`;
  groupdel $unixgroup 2> /dev/null
  groupadd -g $gid $unixgroup || exit 1
done
date

treport

echo "Creating local groups"
for i in `seq 1 1 $NLOCAL`; do
  unixgroup=testlocal$i;
  ntgroup=ntlgrp$i;
  rid=`expr 10000 + $i`;
  testone add rid=$rid unixgroup=$unixgroup ntgroup=$ntgroup type=local || exit 1
done

echo "trying a duplicate add"
testone add rid=10001 unixgroup=testlocal1 ntgroup=foo type=local && exit 1

treport

echo "Creating domain groups"
for i in `seq 1 1 $NGROUP`; do
  unixgroup=testgrp$i;
  ntgroup=ntgrp$i;
  rid=`expr 20000 + $i`;
  testone add rid=$rid unixgroup=$unixgroup ntgroup=$ntgroup type=domain || exit 1
done

treport

echo "Creating builtin groups"
for i in `seq 1 1 $NBUILTIN`; do
  unixgroup=testb$i;
  ntgroup=ntbgrp$i;
  rid=`expr 30000 + $i`;
  testone add rid=$rid unixgroup=$unixgroup ntgroup=$ntgroup type=builtin || exit 1
done

treport

echo "Adding domain groups to local groups"
for i in `seq 1 1 $NLOCAL`; do
 for j in `seq 1 1 $i`; do

  lrid=`expr 10000 + $i`;
  drid=`expr 20000 + $j`;

  testone addmem $DOMSID-$lrid $DOMSID-$drid || exit 1
  ( testone listmem $DOMSID-$lrid | sort -r ) || exit 1
 done
done

echo "trying a duplicate addmem"
testone addmem $DOMSID-10001 $DOMSID-20001 && exit 1

echo "Adding foreign SIDs to local groups"
for i in `seq 1 1 $NLOCAL`; do
 for j in `seq 1 1 $i`; do

  lrid=`expr 10000 + $i`;
  frid=`expr 70000 + $j`;

  testone addmem $DOMSID-$lrid $FORSID-$frid || exit 1
  ( testone listmem $DOMSID-$lrid | sort -r ) || exit 1
 done
done

echo "trying a duplicate foreign addmem"
testone addmem $DOMSID-10001 $FORSID-70001 && exit 1

treport

echo "Listing local group memberships of domain groups"
for i in `seq 1 1 $NGROUP`; do
  rid=`expr 20000 + $i`;
  ( testone memberships $DOMSID-$rid | sort -r ) || exit 1
done

echo "Trying memberships on bogus sid"
testone memberships $DOMSID-999999 || exit 1

treport

testone list | sort

echo "Deleting some domain groups"
for i in `seq 2 2 $NGROUP`; do
  drid=`expr 20000 + $i`;
 testone delete sid=$DOMSID-$drid || exit 1
done

echo "Trying duplicate domain group delete"
testone delete sid=$DOMSID-20002 && exit 1

treport

echo "Deleting some local groups"
for i in `seq 2 4 $NLOCAL`; do
 lrid=`expr 10000 + $i`;
 testone delete sid=$DOMSID-$lrid || exit 1
done

echo "Trying duplicate local group delete"
testone delete sid=$DOMSID-10002 && exit 1

treport

echo "Modifying some domain groups"
for i in `seq 3 2 $NGROUP`; do
  drid=`expr 20000 + $i`;
  testone modify sid=$DOMSID-$drid comment="newcomment-$i" type=domain || exit 1
done

treport

testone list | sort

echo "Listing local group memberships"
for i in `seq 1 1 $NLOCAL`; do
  rid=`expr 20000 + $i`;
  ( testone memberships $DOMSID-$rid | sort -r ) || exit 1
done

treport

echo "Removing some domain groups from local groups"
for i in `seq 1 2 $NLOCAL`; do
 for j in `seq 1 3 $i`; do

  lrid=`expr 10000 + $i`;
  drid=`expr 20000 + $j`;

  testone delmem $DOMSID-$lrid $DOMSID-$drid || exit 1
 done
done

echo "Trying duplicate delmem"
testone delmem $DOMSID-10001 $DOMSID-20001 && exit 1

treport

echo "Listing local group memberships"
for i in `seq 1 1 $NLOCAL`; do
  rid=`expr 20000 + $i`;
  ( testone memberships $DOMSID-$rid | sort -r ) || exit 1
done

treport

echo "Deleting unix groups"
for i in `seq 1 1 $NLOCAL`; do
  unixgroup=testlocal$i;
  groupdel $unixgroup 2> /dev/null
done
for i in `seq 1 1 $NGROUP`; do
  unixgroup=testgrp$i;
  groupdel $unixgroup 2> /dev/null
done
for i in `seq 1 1 $NBUILTIN`; do
  unixgroup=testb$i;
  groupdel $unixgroup 2> /dev/null
done

treport

echo "ALL DONE"
