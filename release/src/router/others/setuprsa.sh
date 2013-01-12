#!/bin/sh

if [ -z "$1" ]; then
	DEST="/tmp/easy-rsa"
else
	if [ -d "$1" ]; then
		DEST=$1/easy-rsa
	else
		echo "Path $1 not found!"
		exit 10
	fi
fi

echo ""
echo "Destination dir: $DEST"
echo ""

mkdir -p $DEST/keys
cp -a /rom/easy-rsa/* $DEST/
chmod a+rx $DEST/*
echo "easy-rsa scripts copied to $DEST."
echo ""
echo "Enter $DEST/, and edit the 'vars' file as needed."
echo "Then, execute it to setup your environment:"
echo " source vars"
echo ""
echo "Finally, initialize things with the following command:"
echo " ./clean-all"
echo ""
echo "Read the easy-rsa documentation on how to use it to generate your certs/keys."

