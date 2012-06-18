#!/bin/sh

if test x$srcdir = x; then
	srcdir=.
fi

if test x$top_builddir = x; then
	top_builddir=..
fi

tmpdir=$top_builddir/tests/tmp/lighttpd

# create test-framework
rm -rf $tmpdir
mkdir -p $tmpdir/servers/www.example.org/pages/
mkdir -p $tmpdir/servers/www.example.org/pages/dummydir/
mkdir -p $tmpdir/servers/www.example.org/pages/go/
mkdir -p $tmpdir/servers/www.example.org/pages/expire/
mkdir -p $tmpdir/servers/www.example.org/pages/indexfile/
mkdir -p $tmpdir/servers/123.example.org/pages/
mkdir -p $tmpdir/logs/
mkdir -p $tmpdir/cache/
mkdir -p $tmpdir/cache/compress/

# copy everything into the right places
cp $srcdir/docroot/www/*.html \
   $srcdir/docroot/www/*.php \
   $srcdir/docroot/www/*.pl \
   $srcdir/docroot/www/*.fcgi \
   $srcdir/docroot/www/*.shtml \
   $srcdir/docroot/www/*.txt $tmpdir/servers/www.example.org/pages/
cp $srcdir/docroot/www/go/*.php $tmpdir/servers/www.example.org/pages/go/
cp $srcdir/docroot/www/expire/*.txt $tmpdir/servers/www.example.org/pages/expire/
cp $srcdir/docroot/www/indexfile/*.php $tmpdir/servers/www.example.org/pages/indexfile/
cp $srcdir/docroot/123/*.txt \
   $srcdir/docroot/123/*.html \
   $srcdir/docroot/123/*.php \
   $srcdir/docroot/123/*.bla $tmpdir/servers/123.example.org/pages/
cp $srcdir/lighttpd.user $tmpdir/
cp $srcdir/lighttpd.htpasswd $tmpdir/
cp $srcdir/var-include-sub.conf $tmpdir/../
touch $tmpdir/servers/www.example.org/pages/image.jpg \
      $tmpdir/servers/www.example.org/pages/image.JPG \
      $tmpdir/servers/www.example.org/pages/Foo.txt \
      $tmpdir/servers/www.example.org/pages/a \
      $tmpdir/servers/www.example.org/pages/index.html~
echo "12345" > $tmpdir/servers/www.example.org/pages/range.pdf

printf "%-40s" "preparing infrastructure"

exit 0
