#!/bin/sh
# Helper script to create CA and server certificates.

srcdir=${1-.}

OPENSSL=@OPENSSL@
CONF=${srcdir}/openssl.conf
REQ="${OPENSSL} req -config ${CONF}"
CA="${OPENSSL} ca -config ${CONF} -batch"
# MKCERT makes a self-signed cert
MKCERT="${REQ} -x509 -new -days 900"

REQDN=reqDN
STRMASK=default
CADIR=./ca
export REQDN STRMASK CADIR

asn1date() {
	date -d "$1" "+%y%m%d%H%M%SZ"
}

openssl version 1>&2

set -ex

for i in ca ca1 ca2 ca3; do
    rm -rf $i
    mkdir $i
    touch $i/index.txt
    echo 01 > $i/serial
    ${OPENSSL} genrsa -rand ${srcdir}/../configure > $i/key.pem
done

${OPENSSL} genrsa -rand ${srcdir}/../configure > client.key

${OPENSSL} dsaparam -genkey -rand ${srcdir}/../configure 1024 > client.dsap
${OPENSSL} gendsa client.dsap > clientdsa.key

${MKCERT} -key ca/key.pem -out ca/cert.pem <<EOF
US
California
Oakland
Neosign
Random Dept
nowhere.example.com
neon@webdav.org
EOF

# Function to generate appropriate output for `openssl req'.
csr_fields() {
CN=${2-"localhost"}
OU=${1-"Neon QA Dept"}
Org=${3-"Neon Hackers Ltd"}
Locality=${4-"Cambridge"}
State=${5-"Cambridgeshire"}
cat <<EOF
GB
${State}
${Locality}
${Org}
${OU}
${CN}
neon@webdav.org
.
.
EOF
}

# Create intermediary CA
csr_fields IntermediaryCA | ${REQ} -new -key ca2/key.pem -out ca2.csr
${CA} -extensions caExt -days 3560 -in ca2.csr -out ca2/cert.pem

csr_fields ExpiredCA | ${REQ} -new -key ca1/key.pem -out ca1/cert.csr

csr_fields NotYetValidCA | ${REQ} -new -key ca3/key.pem -out ca3/cert.csr

CADIR=./ca1 ${CA} -name neoncainit -extensions caExt -startdate `asn1date "2 days ago"` -enddate `asn1date "yesterday"` \
  -in ca1/cert.csr -keyfile ca1/key.pem -out ca1/cert.pem -selfsign

CADIR=./ca3 ${CA} -name neoncainit -extensions caExt -startdate `asn1date "1 year"` -enddate `asn1date "2 years"` \
  -in ca3/cert.csr -keyfile ca3/key.pem -out ca3/cert.pem -selfsign

csr_fields | ${REQ} -new -key ${srcdir}/server.key -out server.csr

csr_fields | ${REQ} -new -key ${srcdir}/server.key -out expired.csr
csr_fields | ${REQ} -new -key ${srcdir}/server.key -out notyet.csr

csr_fields "Upper Case Dept" lOcALhost | \
${REQ} -new -key ${srcdir}/server.key -out caseless.csr

csr_fields "Use AltName Dept" nowhere.example.com | \
${REQ} -new -key ${srcdir}/server.key -out altname1.csr

csr_fields "Two AltName Dept" nowhere.example.com | \
${REQ} -new -key ${srcdir}/server.key -out altname2.csr

csr_fields "Third AltName Dept" nowhere.example.com | \
${REQ} -new -key ${srcdir}/server.key -out altname3.csr

csr_fields "Fourth AltName Dept" localhost | \
${REQ} -new -key ${srcdir}/server.key -out altname4.csr

csr_fields "Good ipAddress altname Dept" nowhere.example.com | \
${REQ} -new -key ${srcdir}/server.key -out altname5.csr

csr_fields "Bad ipAddress altname 1 Dept" nowhere.example.com | \
${REQ} -new -key ${srcdir}/server.key -out altname6.csr

csr_fields "Bad ipAddress altname 2 Dept" nowhere.example.com | \
${REQ} -new -key ${srcdir}/server.key -out altname7.csr

csr_fields "Bad ipAddress altname 3 Dept" nowhere.example.com | \
${REQ} -new -key ${srcdir}/server.key -out altname8.csr

csr_fields "Wildcard Altname Dept 1" | \
${REQ} -new -key ${srcdir}/server.key -out altname9.csr

csr_fields "Bad Hostname Department" nohost.example.com | \
${REQ} -new -key ${srcdir}/server.key -out wrongcn.csr

csr_fields "Self-Signed" | \
${MKCERT} -key ${srcdir}/server.key -out ssigned.pem

# default => T61String
csr_fields "`echo -e 'H\0350llo World'`" localhost |
${REQ} -new -key ${srcdir}/server.key -out t61subj.csr

STRMASK=pkix # => BMPString
csr_fields "`echo -e 'H\0350llo World'`" localhost |
${REQ} -new -key ${srcdir}/server.key -out bmpsubj.csr

STRMASK=utf8only # => UTF8String
csr_fields "`echo -e 'H\0350llo World'`" localhost |
${REQ} -new -key ${srcdir}/server.key -out utf8subj.csr

STRMASK=default

### produce a set of CA certs

csr_fields "First Random CA" "first.example.com" "CAs Ltd." Lincoln Lincolnshire | \
${MKCERT} -key ${srcdir}/server.key -out ca1.pem

csr_fields "Second Random CA" "second.example.com" "CAs Ltd." Falmouth Cornwall | \
${MKCERT} -key ${srcdir}/server.key -out ca2.pem

csr_fields "Third Random CA" "third.example.com" "CAs Ltd." Ipswich Suffolk | \
${MKCERT} -key ${srcdir}/server.key -out ca3.pem

csr_fields "Fourth Random CA" "fourth.example.com" "CAs Ltd." Norwich Norfolk | \
${MKCERT} -key ${srcdir}/server.key -out ca4.pem

cat ca/cert.pem ca[1234].pem > calist.pem

csr_fields "Wildcard Cert Dept" "*.example.com" | \
${REQ} -new -key ${srcdir}/server.key -out wildcard.csr

csr_fields "Wildcard IP Cert" "*.0.0.1" | \
${REQ} -new -key ${srcdir}/server.key -out wildip.csr

csr_fields "Neon Client Cert" ignored.example.com | \
${REQ} -new -key client.key -out client.csr

csr_fields "Neon Client Cert" ignored.example.com | \
${REQ} -new -key clientdsa.key -out clientdsa.csr

### requests using special DN.

REQDN=reqDN.doubleCN
csr_fields "Double CN Dept" "nohost.example.com
localhost" | ${REQ} -new -key ${srcdir}/server.key -out twocn.csr

REQDN=reqDN.CNfirst
echo localhost | ${REQ} -new -key ${srcdir}/server.key -out cnfirst.csr

REQDN=reqDN.missingCN
echo GB | ${REQ} -new -key ${srcdir}/server.key -out missingcn.csr

REQDN=reqDN.justEmail
echo blah@example.com | ${REQ} -new -key ${srcdir}/server.key -out justmail.csr

# presume AVAs will come out in least->most specific order still...
REQDN=reqDN.twoOU
csr_fields "Second OU Dept
First OU Dept" | ${REQ} -new -key ${srcdir}/server.key -out twoou.csr

### don't put ${REQ} invocations after here

for f in server client clientdsa twocn caseless cnfirst \
    t61subj bmpsubj utf8subj \
    missingcn justmail twoou wildcard wildip wrongcn; do
  ${CA} -days 900 -in ${f}.csr -out ${f}.cert
done

${CA} -startdate `asn1date "2 days ago"` -enddate `asn1date "yesterday"` -in expired.csr -out expired.cert

${CA} -startdate `asn1date "tomorrow"` -enddate `asn1date "2 days"` -in notyet.csr -out notyet.cert

for n in 1 2 3 4 5 6 7 8 9; do
 ${CA} -extensions altExt${n} -days 900 \
     -in altname${n}.csr -out altname${n}.cert
done

# Sign this CSR using the intermediary CA
CADIR=./ca2 ${CA} -days 900 -in server.csr -out ca2server.cert
# And create a file with the concatenation of both EE and intermediary
# cert.
cat ca2server.cert ca2/cert.pem > ca2server.pem
 
# sign with expired CA
CADIR=./ca1 ${CA} -days 3 -in server.csr -out ca1server.cert

# sign with not yet valid CA
CADIR=./ca3 ${CA} -days 3 -in server.csr -out ca3server.cert

MKPKCS12="${OPENSSL} pkcs12 -export -passout stdin -in client.cert -inkey client.key"

# generate a PKCS12 cert from the client cert: -passOUT because it's the
# passphrase on the OUTPUT cert, confusing...
echo foobar | ${MKPKCS12} -name "Just A Neon Client Cert" -out client.p12

# generate a PKCS#12 cert with no password and a friendly name
echo | ${MKPKCS12} -name "An Unencrypted Neon Client Cert" -out unclient.p12

# PKCS#12 cert with DSA key
echo | ${OPENSSL} pkcs12 -name "An Unencrypted Neon DSA Client Cert" \
    -export -passout stdin \
    -in clientdsa.cert -inkey clientdsa.key \
    -out dsaclient.p12

# generate a PKCS#12 cert with no friendly name
echo | ${MKPKCS12} -out noclient.p12

# generate a PKCS#12 cert with no private keys
echo | ${MKPKCS12} -nokeys -out nkclient.p12

# generate a PKCS#12 cert without the cert
echo | ${MKPKCS12} -nokeys -out ncclient.p12

# generate an encoded PKCS#12 cert with no private keys
echo foobar | ${MKPKCS12} -nokeys -out enkclient.p12

# a PKCS#12 cert including a bundled CA cert
echo foobar | ${MKPKCS12} -certfile ca/cert.pem -name "A Neon Client Cert With CA" -out clientca.p12

### a file containing a complete chain

cat ca/cert.pem server.cert > chain.pem

### NSS database initialization, for testing PKCS#11.
CERTUTIL=@CERTUTIL@
PK12UTIL=@PK12UTIL@

if [ ${CERTUTIL} != "notfound" -a ${PK12UTIL} != "notfound" ]; then
  rm -rf nssdb nssdb-dsa
  mkdir nssdb nssdb-dsa

  echo foobar > nssdb.pw

  ${CERTUTIL} -d nssdb -N -f nssdb.pw
  ${PK12UTIL} -d nssdb -K foobar -W '' -i unclient.p12

  ${CERTUTIL} -d nssdb-dsa -N -f nssdb.pw
  ${PK12UTIL} -d nssdb-dsa -K foobar -W '' -i dsaclient.p12

  rm -f nssdb.pw
fi
