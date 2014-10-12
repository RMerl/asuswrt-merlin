#!/bin/bash

# rebuild our heimdal lex/yacc files. Run this manually if you update heimdal

lexfiles="heimdal/lib/asn1/lex.l heimdal/lib/hx509/sel-lex.l heimdal/lib/com_err/lex.l"
yaccfiles="heimdal/lib/asn1/asn1parse.y heimdal/lib/hx509/sel-gram.y heimdal/lib/com_err/parse.y"

set -e

LEX="lex"
YACC="yacc"

top=$PWD

call_lex() {
    lfile="$1"

    echo "Calling $LEX on $lfile"

    dir=$(dirname $lfile)
    base=$(basename $lfile .l)
    cfile=$base".c"
    lfile=$base".l"

    cd $dir

    $LEX $lfile || exit 1

    if [ -r lex.yy.c ]; then
	echo "#include \"config.h\"" > $base.c
	sed -e "s|lex\.yy\.c|$cfile|" lex.yy.c >> $base.c
	rm -f $base.yy.c
    elif [ -r $base.yy.c ]; then
	echo "#include \"config.h\"" > $base.c
	sed -e "s|$base\.yy\.c|$cfile|" $base.yy.c >> $base.c
	rm -f $base.yy.c
    elif [ -r $base.c ]; then
	mv $base.c $base.c.tmp
	echo "#include \"config.h\"" > $base.c
	sed -e "s|$base\.yy\.c|$cfile|" $base.c.tmp >> $base.c
	rm -f $base.c.tmp
    elif [ ! -r base.c ]; then
	echo "$base.c nor $base.yy.c nor lex.yy.c generated."
	exit 1
    fi
    cd $top
}


call_yacc() {
    yfile="$1"

    echo "Calling $YACC on $yfile"

    dir=$(dirname $yfile)
    base=$(basename $yfile .y)
    cfile=$base".c"
    yfile=$base".y"

    cd $dir

    $YACC -d $yfile || exit 1
    if [ -r y.tab.h -a -r y.tab.c ];then
	sed -e "/^#/!b" -e "s|y\.tab\.h|$cfile|" -e "s|\"$base.y|\"$cfile|"  y.tab.h > $base.h
	sed -e "s|y\.tab\.c|$cfile|" -e "s|\"$base.y|\"$cfile|" y.tab.c > $base.c
	rm -f y.tab.c y.tab.h
    elif [ ! -r $base.h -a ! -r $base.c]; then
	echo "$base.h nor $base.c generated."
	exit 1
    fi
    cd $top
}



for lfile in $lexfiles; do
    call_lex $lfile
done

for yfile in $yaccfiles; do
    call_yacc $yfile
done
