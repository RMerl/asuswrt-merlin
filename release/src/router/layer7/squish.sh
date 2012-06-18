#!/bin/bash
#
# strip out anything but the required lines and put everything in one spot - zzz
#

if [ ! -s squished/squish-done ]; then
	rm -rf squished
	mkdir squished

	for f in {protocols/*/*.pat,*.pat}; do
		n=`basename $f`
		[ ! -f $f ] && break
		echo -en "Squishing: $n                \r"
		grep -v "^\s*$\|^#\|^userspace " $f > squished/$n
		if [ `wc -l squished/$n | cut -d ' ' -f 1` -ne 2 ]; then
			echo "error while squishing $f..."
			exit 1
		fi
	done

	rm -f squished/unknown
	echo 1 > squished/squish-done
fi

echo "L7 filters squished."
