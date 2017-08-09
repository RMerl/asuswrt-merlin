#!/bin/bash

set -x

. `dirname $0`/vars

if [ -z "$site" ]; then
    site="Default-First-Site-Name"
fi

bin/ldbdel -r -H ldap://$server.$DNSDOMAIN -U$workgroup/administrator%$pass "CN=$machine,CN=Computers,$dn"
bin/ldbdel -r -H ldap://$server.$DNSDOMAIN -U$workgroup/administrator%$pass "CN=$machine,OU=Domain Controllers,$dn"
bin/ldbdel -r -H ldap://$server.$DNSDOMAIN -U$workgroup/administrator%$pass "CN=$machine,CN=Servers,CN=$site,CN=Sites,CN=Configuration,$dn"
rm -f $PREFIX/private/*.ldb
