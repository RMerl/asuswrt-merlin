#!/bin/sh

prefix1="$1"
prefix2="$2"

(cd $prefix1 && find . ) | sort > p1.txt
(cd $prefix2 && find . ) | sort > p2.txt
diff -u p[12].txt
