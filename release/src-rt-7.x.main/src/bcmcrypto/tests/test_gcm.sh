#!/bin/sh

tmpfile=/tmp/gcm_test_sh_tmp.$$
table_sizes="0 4096 256"
for tsz in $table_sizes
do
	echo "Test with table size $tsz"
	echo -n "	Compiling.."
		make -f gcm_test.mk  GCM_TABLE_SZ=$tsz clean
		make -f gcm_test.mk  GCM_TABLE_SZ=$tsz
	echo "done."
	echo -n "	Running.."
		./gcm_test > $tmpfile 2>&1
		grep -i error $tmpfile > /dev/null 2>&1
		if [ $? = 0 ] ;  then
			echo "Test Failed"
			cat $tmpfile
			break;
		fi
	echo "done."
done

echo -n "	Cleaning up.."
echo rm -f $tmpfile
rm -f $tmpfile
make -f gcm_test.mk  clean
echo "done."
exit 0
