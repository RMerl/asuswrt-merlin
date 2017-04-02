#!/bin/sh

CERTFILE=/etc/cert.pem
KEYFILE=/etc/key.pem

if [ -n "$1" ] && [ ! -f "$1" ]; then
	echo "Usage: $(basename "$0") [CERTFILE] [KEYFILE]"
	echo
	echo "CERTFILE and KEYFILE default to /etc/[cert|key].pem if not specified"
	exit 0
fi

#pre-flight check
for app in openssl nvram service tar diff; do
	if ! hash $app 2> /dev/null; then
		echo "ERROR: '$app' is required but could not be found"
		exit 1
	fi
done

install_cert() {
	echo "Verifying certificate..."
	if [ ! -f "$1" ]; then
		echo "ERROR: Certificate not found in '$1'"
		exit 1
	# Verify the cert is valid and write it to CERTFILE
	elif ! openssl x509 -in "$1" -out "$CERTFILE">/dev/null 2>&1; then
		echo "ERROR: Invalid certificate format in '$1'"
		exit 1
	fi
}

install_key() {
	echo "Verifying private key..."
	if [ ! -f "$1" ]; then
		echo "ERROR: Private key not found in '$1'"
		exit 1
	# Verify the key is valid, remove any password, and write the new key
	elif ! openssl rsa -in "$1" -out "$KEYFILE" >/dev/null 2>&1; then
		echo "ERROR: Invalid private key format in '$1'"
		exit 1
	fi
}

if [ -n "$1" ]; then
	install_cert "$1"
else
	install_cert "$CERTFILE"
fi

if [ -n "$2" ]; then
	install_key "$2"
else
	install_key "$KEYFILE"
fi


if [ $(nvram get https_crt_save) -eq 0 ]; then
	echo "Enabling certificate saving..."
	nvram set https_crt_save=1
fi

if [ -n "$(nvram get https_crt_file)" ]; then
	echo "Clearing out old certificate..."
	nvram unset https_crt_file
fi

echo "Restarting httpd..."
service restart_httpd > /dev/null
# give nvram a chance to catch up
sleep 1

echo -en "\nVerifying installed certificate..."

# give nvram a chance to catch up
sleep 1

if nvram get https_crt_file | openssl base64 -d -A | tar xOz etc/cert.pem | diff -q "$CERTFILE" - > /dev/null; then
	echo
else
	echo "failed"
	exit 1
fi

echo -n "Verifying installed private key..."
if nvram get https_crt_file | openssl base64 -d -A | tar xOz etc/key.pem | diff -q "$KEYFILE" - > /dev/null; then
	echo
else
	echo "failed"
	exit 1
fi

echo -e "\nCertificate successfully installed"
