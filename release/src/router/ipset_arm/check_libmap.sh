#!/bin/sh
for file in include/libipset/*.h; do
    case $file in
    */ui.h) continue ;;
    esac
    grep ^extern $file | sed -r -e 's/\(.*//' -e 's/.* \*?//' | egrep -v '\[|\;'
done | while read symbol; do
    if [ -z "$symbol" -o "$symbol" = '{' ]; then
    	continue
    fi
    if [ -z "`grep \" $symbol\;\" lib/libipset.map`" ]; then
    	echo "Symbol $symbol is missing from libipset.map"
    fi
done
