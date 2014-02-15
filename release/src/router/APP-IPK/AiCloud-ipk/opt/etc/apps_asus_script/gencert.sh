#!/bin/sh
SECS=1262278080

cd /etc

NVCN=`nvram get https_crt_cn`
if [ "$NVCN" == "" ]; then
	NVCN=`nvram get lan_ipaddr`
fi

cp -L openssl.cnf openssl.config

I=0
for CN in $NVCN; do
        echo "$I.commonName=CN" >> openssl.config
        echo "$I.commonName_value=$CN" >> openssl.config
        I=$(($I + 1))
done

# create the key and certificate request
aicloud_openssl req -new -out /tmp/cert.csr -config openssl.config -keyout /tmp/privkey.pem -newkey rsa:1024 -passout pass:password
# remove the passphrase from the key
aicloud_openssl rsa -in /tmp/privkey.pem -out key.pem -passin pass:password
# convert the certificate request into a signed certificate
aicloud_openssl x509 -in /tmp/cert.csr -out cert.pem -req -signkey key.pem -setstartsecs $SECS -days 3653 -set_serial $1

#	openssl x509 -in /etc/cert.pem -text -noout

# server.pem for WebDav SSL
cat key.pem cert.pem > server.pem
#cp -f server.pem /etc
rm -f /tmp/cert.csr /tmp/privkey.pem openssl.config
