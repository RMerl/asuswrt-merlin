#!/bin/sh

# compare the generated files from a waf

old_build=$HOME/samba_old

gen_files=$(cd bin/default && find . -type f -name '*.[ch]')

2>&1

strip_file()
{
    in_file=$1
    out_file=$2
    cat $in_file |
                   grep -v 'The following definitions come from' |
		   grep -v 'Automatically generated at' |
		   grep -v 'Generated from' |
                   sed 's|/home/tnagy/samba/source4||g' |
                   sed 's|/home/tnagy/samba/|../|g' |
                   sed 's|bin/default/source4/||g' |
                   sed 's|bin/default/|../|g' |
                   sed 's/define _____/define ___/g' |
                   sed 's/define __*/define _/g' |
                   sed 's/define _DEFAULT_/define _/g' |
                   sed 's/define _SOURCE4_/define ___/g' |
                   sed 's/define ___/define _/g' |
                   sed 's/ifndef ___/ifndef _/g' |
                   sed 's|endif /* ____|endif /* __|g' |
		   sed s/__DEFAULT_SOURCE4/__/ |
                   sed s/__DEFAULT_SOURCE4/__/ |
		   sed s/__DEFAULT/____/  	   > $out_file
}

compare_file()
{
    f=$f
    bname=$(basename $f)
    t1=/tmp/$bname.old.$$
    t2=/tmp/$bname.new.$$
    strip_file $old_build/$f $t1
    strip_file bin/default/$f     $t2
    diff -u -b $t1 $t2 2>&1
    rm -f $t1 $t2
}

for f in $gen_files; do
    compare_file $f
done

