#!/bin/bash

set -x

. `dirname $0`/vars

`dirname $0`/vampire_ad.sh || exit 1

ntds_guid=$(sudo bin/ldbsearch -H $PREFIX/private/sam.ldb -b "CN=NTDS Settings,CN=$machine,CN=Servers,CN=Default-First-Site-Name,CN=Sites,CN=Configuration,$dn" objectGUID|grep ^objectGUID| awk '{print $2}')

cp $PREFIX/private/$DNSDOMAIN.zone{.template,}
sed -i "s/NTDSGUID/$ntds_guid/g" $PREFIX/private/$DNSDOMAIN.zone
cp $PREFIX/private/named.conf{.local,}
sudo rndc reconfig
fsmotmp=`mktemp fsmo.ldif.XXXXXXXXX`
cp `dirname $0`/fsmo.ldif.template $fsmotmp
sed -i "s/NTDSGUID/$ntds_guid/g" $fsmotmp
sed -i "s/MACHINE/$machine/g" $fsmotmp
sed -i "s/DNSDOMAIN/$DNSDOMAIN/g" $fsmotmp
sed -i "s/BASEDN/$dn/g" $fsmotmp
sed -i "s/NETBIOSDOMAIN/$workgroup/g" $fsmotmp
sudo bin/ldbmodify -H $PREFIX/private/sam.ldb $fsmotmp
rm $fsmotmp
