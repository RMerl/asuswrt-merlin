#!/bin/sh
#
# Tar up a releasable archive
# $Id: make-release.sh 4263 2012-05-24 15:55:53Z themiron.ru $

VERSION=`grep '^VERSION=' Makefile.in | sed -e 's/VERSION=//'`

if test "$VERSION" = "" ; then
    echo "Doh!  Could not figure out version from Makefile.in"
    exit 1
fi

# In DFS's tree, libevent is in parent directory.  Create symlink
# if needed

test -d libevent || ln -s ../libevent . || exit 1

MANIFEST="README Makefile.in install-sh auth.c config.guess config.sub configure configure.in debug.c dgram.c l2tp.conf l2tp.h main.c make-release.sh md5.c md5.h network.c options.c peer.c session.c tunnel.c utils.c handlers/Makefile.in handlers/cmd-control.c handlers/cmd.c handlers/dstring.c handlers/dstring.h handlers/pty.c handlers/sync-pppd.c man/l2tpd.8 man/l2tp.conf.5 libevent/Makefile.in libevent/event.c libevent/event.h libevent/event_sig.c libevent/event_tcp.c libevent/event_tcp.h libevent/eventpriv.h libevent/hash.c libevent/hash.h libevent/Doc/flow.fig libevent/Doc/libevent.tex libevent/Doc/style.tex libevent/Doc/libevent.pdf"

DIR=rp-l2tp-$VERSION
PWD=`pwd`
test -d $DIR && rm -rf $DIR
mkdir $DIR || exit 1
for i in $MANIFEST ; do
    echo "Doing $i..."
    d=`dirname $i`
    test -d $DIR/$d || mkdir -p $DIR/$d || exit 1
    ln -s $PWD/$i $DIR/$d || exit 1
done
exit 0
