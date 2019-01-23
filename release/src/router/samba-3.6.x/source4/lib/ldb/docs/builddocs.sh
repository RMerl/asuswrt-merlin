#!/bin/sh
# build ldb docs
# tridge@samba.org August 2006

XSLTPROC="$1"
SRCDIR="$2"

if [ -z "$XSLTPROC" ] || [ ! -x "$XSLTPROC" ]; then
    echo "xsltproc not installed"
    exit 0
fi

MANXSL="http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl"
HTMLXSL="http://docbook.sourceforge.net/release/xsl/current/html/docbook.xsl"

mkdir -p man

for f in $SRCDIR/man/*.xml; do
    base=`basename $f .xml`
    out=man/"`basename $base`"
    if [ ! -f "$out" ] || [ "$f" -nt "$out" ]; then
	echo Processing manpage $f
	$XSLTPROC --nonet -o "$out" "$MANXSL" $f
	ret=$?
	if [ "$ret" = "4" ]; then
	    echo "ignoring stylesheet error 4 for $MANXSL"
	    exit 0
	fi
	if [ "$ret" != "0" ]; then
	    echo "xsltproc failed with error $ret"
	    exit $ret
	fi
    fi
done

for f in $SRCDIR/man/*.xml; do
    base=`basename $f .xml`
    out=man/"`basename $base`".html
    if [ ! -f "$out" ] || [ "$f" -nt "$out" ]; then
	echo Processing html $f
	$XSLTPROC --nonet -o "$out" "$HTMLXSL" $f
	ret=$?
	if [ "$ret" = "4" ]; then
	    echo "ignoring stylesheet error 4 for $HTMLXSL"
	    exit 0
	fi
	if [ "$ret" != "0" ]; then
	    echo "xsltproc failed with error $ret"
	    exit $ret
	fi
    fi
done
