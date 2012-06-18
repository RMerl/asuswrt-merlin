#!/bin/sh
#set -x

MKSQ=../src/linux/linux/scripts/squashfs/mksquashfs
TRX=../tools/trx
ADDLANG=../tools/addlang

LANG="sc sp sw it de fr en"

if [ a$1 != a ] ; then
LANG=$1
fi

rm -rf lang_img
mkdir lang_img 

# Handle original EN language
#$MKSQ ../src/router/www/cisco_wrt54g_en oen.sq
#$TRX -o oen.trx oen.sq
#$ADDLANG -i oen.trx -o lang_img/oen_lang.bin

#rm -rf /tmp/www_m
#cp -rfa ../src/router/mipsel-uclibc/target/www/ /tmp/www_m
#rm -rf /tmp/www_m/lang_pack
#rm -rf /tmp/www_m/help

# Handle multiple language
for lang in $LANG ; do 
	#echo "lang=$lang"
	
	if [ ! -d ../src/router/www/lang_pack/${lang}_lang_pack/ ] ; then
		echo "Cann't find \"$lang\" language package."
		exit
	fi

	rm -rf /tmp/www_m
	cp -rfa ../src/router/mipsel-uclibc/target/www/ /tmp/www_m
	rm -rf /tmp/www_m/lang_pack
	rm -rf /tmp/www_m/help

	cp -rfa ../src/router/www/lang_pack/${lang}_lang_pack/ /tmp/www_m/lang_pack
	cp -rfa ../src/router/www/lang_pack/all_help/${lang}_help /tmp/www_m/help

	test -d ../src/router/www/lang_pack/${lang}_image/ && cp -rfa ../src/router/www/lang_pack/${lang}_image/*.gif /tmp/www_m/image/

	$MKSQ /tmp/www_m 2${lang}.sq 
done

for lang in $LANG ; do 
	$TRX -o 2${lang}.trx 2${lang}.sq
	$ADDLANG -i 2${lang}.trx -o lang_img/${lang}_lang.bin
done

rm -rf /tmp/www_m
rm -f *.trx
rm -f *.sq

