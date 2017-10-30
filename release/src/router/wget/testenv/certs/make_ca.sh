#!/bin/sh -e

# create a self signed CA certificate
certtool --generate-privkey --outfile ca-key.pem
certtool --generate-self-signed --load-privkey ca-key.pem --template=ca-template.cfg --outfile ca-cert.pem

# create the server RSA private key
certtool --generate-privkey --outfile server-key.pem --rsa

# generate a server certificate using the private key only
certtool --generate-certificate --load-privkey server-key.pem --template=server-template.cfg --outfile server-cert.pem --load-ca-certificate ca-cert.pem --load-ca-privkey ca-key.pem

# create a CRL for the server certificate
certtool --generate-crl --load-ca-privkey ca-key.pem --load-ca-certificate ca-cert.pem --load-certificate server-cert.pem --outfile server-crl.pem --template=server-template.cfg

# generate a public key in PEM format
openssl x509 -noout -pubkey < server-cert.pem > server-pubkey.pem

# generate a public key in DER format
openssl x509 -noout -pubkey < server-cert.pem | openssl asn1parse -noout -inform pem -out server-pubkey.der

# generate a sha256 hash of the public key
openssl x509 -noout -pubkey < server-cert.pem | openssl asn1parse -noout -inform pem -out /dev/stdout | openssl dgst -sha256 -binary | openssl base64 > server-pubkey-sha256.base64
