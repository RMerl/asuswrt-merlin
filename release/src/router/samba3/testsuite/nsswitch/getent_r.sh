#!/bin/sh
#
# Verify test output.  Basically we check to see if all the files generated
# in /tmp by the get{pw,gr}ent_r.c and program are identical.  If there is
# some problem with the re-entrancy of the code then the information in the
# two files will be different.  
#

TYPE=$1
ID=$2
FILES="/tmp/${TYPE}_r-${ID}.out-*"

# Sort files

for file in $FILES; do
    cat $file | sort > $file.sorted
done

# Diff files

SORTED="/tmp/${TYPE}_r-${ID}.out-*.sorted"
failed=0

for file1 in $SORTED; do
    for file2 in $SORTED; do
        if [ $file1 != $file2 ]; then
                diff $file1 $file2
        fi
    done
done

# Clean up

rm -f $SORTED

