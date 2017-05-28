#!/bin/sh
SECS=1262278080

WAITTIMER=0
while [ -f "/var/run/gencert.pid" -a $WAITTIMER -lt 14 ]
do
	WAITTIMER=$((WAITTIMER+2))
	sleep $WAITTIMER
done
touch /var/run/gencert.pid

cd /etc

cp -L openssl.cnf openssl.config

LANCN=$(nvram get https_crt_cn)
LANIP=$(nvram get lan_ipaddr)

if [ "$LANCN" != "" ]
then
	I=0
	for CN in $LANCN; do
		echo "$I.commonName=CN" >> openssl.config
		echo "$I.commonName_value=$CN" >> openssl.config
		echo "$I.organizationName=O" >> /etc/openssl.config
		echo "$I.organizationName_value=$(uname -o)" >> /etc/openssl.config
		I=$(($I + 1))
	done
else
	echo "0.commonName=CN" >> openssl.config
	echo "0.commonName_value=$LANIP" >> openssl.config
	echo "0.organizationName=O" >> /etc/openssl.config
	echo "0.organizationName_value=$(uname -o)" >> /etc/openssl.config
fi

I=0
# Start of SAN extensions
sed -i "/\[ CA_default \]/acopy_extensions = copy" openssl.config
sed -i "/\[ v3_ca \]/asubjectAltName = @alt_names" openssl.config
sed -i "/\[ v3_req \]/asubjectAltName = @alt_names" openssl.config
echo "[alt_names]" >> openssl.config

# IP
echo "IP.0 = $LANIP" >> openssl.config
echo "DNS.$I = $LANIP" >> openssl.config # For broken clients like IE
I=$(($I + 1))

# DUT
echo "DNS.$I = router.asus.com" >> openssl.config
I=$(($I + 1))

# User-defined CN (if we have any)
if [ "$LANCN" != "" ]
then
	for CN in $LANCN; do
		echo "DNS.$I = $CN" >> openssl.config
		I=$(($I + 1))
	done
fi

# hostnames

LANDOMAIN=$(nvram get lan_domain)
COMPUTERNAME=$(nvram get computer_name)
LANHOSTNAME=$(nvram get lan_hostname)

if [ "$COMPUTERNAME" != "" ]
then
	echo "DNS.$I = $COMPUTERNAME" >> openssl.config
	I=$(($I + 1))

	if [ "$LANDOMAIN" != "" ]
	then
		echo "DNS.$I = $COMPUTERNAME.$LANDOMAIN" >> openssl.config
		I=$(($I + 1))
	fi
fi

if [ "$LANHOSTNAME" != "" ]
then
	echo "DNS.$I = $LANHOSTNAME" >> openssl.config
	I=$(($I + 1))

	if [ "$LANDOMAIN" != "" ]
	then
		echo "DNS.$I = $LANHOSTNAME.$LANDOMAIN" >> openssl.config
		I=$(($I + 1))
	fi
fi


# DDNS
DDNSHOSTNAME=$(nvram get ddns_hostname_x)
DDNSSERVER=$(nvram get ddns_server_x)
DDNSUSER=$(nvram get ddns_username_x)

if [ "$(nvram get ddns_enable_x)" == "1" -a "$DDNSSERVER" != "WWW.DNSOMATIC.COM" -a "$DDNSHOSTNAME" != "" ]
then
	if [ "$DDNSSERVER" == "WWW.NAMECHEAP.COM" -a "$DDNSUSER" != "" ]
	then
		echo "DNS.$I = $DDNSHOSTNAME.$DDNSUSER" >> openssl.config
		I=$(($I + 1))
	else
		echo "DNS.$I = $DDNSHOSTNAME" >> openssl.config
		I=$(($I + 1))
	fi
fi


# create the key
openssl genrsa -out key.pem 2048 -config /etc/openssl.config
# create certificate request and sign it
openssl req -new -x509 -key key.pem -sha256 -out cert.pem -days 3653 -config /etc/openssl.config


#	openssl x509 -in /etc/cert.pem -text -noout

# server.pem for WebDav SSL
cat key.pem cert.pem > server.pem

rm -f /tmp/cert.csr /etc/openssl.config /var/run/gencert.pid
