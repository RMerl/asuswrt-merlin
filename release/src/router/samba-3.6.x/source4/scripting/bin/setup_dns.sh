#!/bin/bash
# example script to setup DNS for a vampired domain

[ $# = 3 ] || {
    echo "Usage: setup_dns.sh HOSTNAME DOMAIN IP"
    exit 1
}

HOSTNAME="$(echo $1 | tr '[a-z]' '[A-Z]')"
DOMAIN="$(echo $2 | tr '[a-z]' '[A-Z]')"
IP="$3"

RSUFFIX=$(echo $DOMAIN | sed s/[\.]/,DC=/g)

[ -z "$PRIVATEDIR" ] && {
    PRIVATEDIR=$(bin/testparm --section-name=global --parameter-name='private dir' --suppress-prompt 2> /dev/null)
}

OBJECTGUID=$(bin/ldbsearch -s base -H "$PRIVATEDIR/sam.ldb" -b "CN=NTDS Settings,CN=$HOSTNAME,CN=Servers,CN=Default-First-Site-Name,CN=Sites,CN=Configuration,DC=$RSUFFIX" objectguid|grep ^objectGUID| cut -d: -f2)

echo "Found objectGUID $OBJECTGUID"

echo "Running kinit for $HOSTNAME\$@$DOMAIN"
bin/samba4kinit -e arcfour-hmac-md5 -k -t "$PRIVATEDIR/secrets.keytab" $HOSTNAME\$@$DOMAIN || exit 1
echo "Adding $HOSTNAME.$DOMAIN"
scripting/bin/nsupdate-gss --noverify $HOSTNAME $DOMAIN $IP 300 || {
    echo "Failed to add A record"
    exit 1
}
echo "Adding $OBJECTGUID._msdcs.$DOMAIN => $HOSTNAME.$DOMAIN"
scripting/bin/nsupdate-gss --realm=$DOMAIN --noverify --ntype="CNAME" $OBJECTGUID _msdcs.$DOMAIN $HOSTNAME.$DOMAIN 300 || {
    echo "Failed to add CNAME"
    exit 1
}
echo "Checking"
rndc flush
host $HOSTNAME.$DOMAIN
host $OBJECTGUID._msdcs.$DOMAIN
