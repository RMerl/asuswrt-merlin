#!/bin/sh
if [ "$1" == "" ]
then
	SERVICE="web"
else
	SERVICE=$1
fi

SECS=1262278080

WAITTIMER=0
while [ -f "/var/run/gencert.pid" -a $WAITTIMER -lt 14 ]
do
	WAITTIMER=$((WAITTIMER+2))
	sleep $WAITTIMER
done
touch /var/run/gencert.pid

if [ "$SERVICE" == "ftp" ]
then
	cd /jffs/ssl/
	KEYNAME="ftp.key"
	CERTNAME="ftp.crt"
else
	cd /etc
	KEYNAME="key.pem"
	CERTNAME="cert.pem"
fi

cp -L /etc/openssl.cnf /etc/openssl.config

LANCN=$(nvram get https_crt_cn)
LANIP=$(nvram get lan_ipaddr)

if [ "$LANCN" != "" ]
then
	I=0
	for CN in $LANCN; do
		echo "$I.commonName=CN" >> /etc/openssl.config
		echo "$I.commonName_value=$CN" >> /etc/openssl.config
		echo "$I.organizationName=O" >> /etc/openssl.config
		echo "$I.organizationName_value=$(uname -o)" >> /etc/openssl.config
		I=$(($I + 1))
	done
else
	echo "0.commonName=CN" >> /etc/openssl.config
	echo "0.commonName_value=$LANIP" >> /etc/openssl.config
	echo "0.organizationName=O" >> /etc/openssl.config
	echo "0.organizationName_value=$(uname -o)" >> /etc/openssl.config
fi

I=0
# Start of SAN extensions
sed -i "/\[ CA_default \]/acopy_extensions = copy" /etc/openssl.config
sed -i "/\[ v3_ca \]/asubjectAltName = @alt_names" /etc/openssl.config
sed -i "/\[ v3_req \]/asubjectAltName = @alt_names" /etc/openssl.config
echo "[alt_names]" >> /etc/openssl.config

# IP
echo "IP.0 = $LANIP" >> /etc/openssl.config
echo "DNS.$I = $LANIP" >> /etc/openssl.config # For broken clients like IE
I=$(($I + 1))

# DUT
echo "DNS.$I = router.asus.com" >> /etc/openssl.config
I=$(($I + 1))

# User-defined CN (if we have any)
if [ "$LANCN" != "" ]
then
	for CN in $LANCN; do
		echo "DNS.$I = $CN" >> /etc/openssl.config
		I=$(($I + 1))
	done
fi

# hostnames

LANDOMAIN=$(nvram get lan_domain)
COMPUTERNAME=$(nvram get computer_name)
LANHOSTNAME=$(nvram get lan_hostname)

if [ "$COMPUTERNAME" != "" ]
then
	echo "DNS.$I = $COMPUTERNAME" >> /etc/openssl.config
	I=$(($I + 1))

	if [ "$LANDOMAIN" != "" ]
	then
		echo "DNS.$I = $COMPUTERNAME.$LANDOMAIN" >> /etc/openssl.config
		I=$(($I + 1))
	fi
fi

if [ "$LANHOSTNAME" != "" ]
then
	echo "DNS.$I = $LANHOSTNAME" >> /etc/openssl.config
	I=$(($I + 1))

	if [ "$LANDOMAIN" != "" ]
	then
		echo "DNS.$I = $LANHOSTNAME.$LANDOMAIN" >> /etc/openssl.config
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
		echo "DNS.$I = $DDNSHOSTNAME.$DDNSUSER" >> /etc/openssl.config
		I=$(($I + 1))
	else
		echo "DNS.$I = $DDNSHOSTNAME" >> /etc/openssl.config
		I=$(($I + 1))
	fi
fi


# create the key
openssl genrsa -out $KEYNAME 2048 -config /etc/openssl.config
# create certificate request and sign it
openssl req -new -x509 -key $KEYNAME -sha256 -out $CERTNAME -days 3653 -config /etc/openssl.config


#	openssl x509 -in /etc/$CERTNAME -text -noout

if [ "$SERVICE" == "web" ]
then
	# server.pem for WebDav SSL
	cat $KEYNAME $CERTNAME > server.pem
fi

rm -f /tmp/cert.csr /etc/openssl.config /var/run/gencert.pid
