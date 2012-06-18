#!/bin/sh
# make an index in the certificate directory
#

usage () {
	echo "usage: lprng_index_certs [certdir]" >&2
	echo "  default directory $CA_DIR" >&2
	exit 1
}

ssl_program=@OPENSSL@
CA_CERT=@SSL_CA_FILE@

case "$1" in
	--TEMP=* )
		CA_DIR=`expr "$1" : "--TEMP=\(.*\)"`
		shift
		;;
esac


if [ "$1" != "" ] ; then
	CA_DIR=$1
fi

if [ "$CA_DIR" = "" ] ; then
	CA_DIR=${CA_CERT}
fi

if [ -f $CA_DIR ] ; then
	CA_DIR=`dirname ${CA_DIR}`
fi

if [ ! -d $CA_DIR ] ; then
	echo "directory $CA_DIR does not exist" >&2
	usage
fi

set -e
cd $CA_DIR

for file in *.crt; do
	if [ ".`grep SKIPME $file`" != . ]; then
		echo dummy | awk '{ printf("%-15s ... Skipped\n", file); }' "file=$file";
	else
		n=0;
		while [ 1 ]; do
			hash="`$ssl_program x509 -noout -hash <$file`";
			if [ -r "$hash.$n" ]; then
				n=`expr $n + 1`;
			else
				echo dummy | awk '{ printf("%-15s ... %s\n", file, hash); }' "file=$file" "hash=$hash.$n";
				ln -s $file $hash.$n;
				break;
			fi;
		done;
	fi;
done
