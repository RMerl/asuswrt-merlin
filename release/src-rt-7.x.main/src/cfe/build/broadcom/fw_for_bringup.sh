#!/bin/sh

board_type="$1"
commondir=/home/cgd/proj/sb/systemsw/screening/autotest/tests/cfe_common

install_board_type="$board_type"
case "$board_type" in
cswarm)
	chip_types="bcm1250"
	fw_types="diag_be diag_le"
	fw_types="$fw_types diag3e_be diag3e_le"
	fw_types="$fw_types general_be general_le"
	;;
cswarm1125wb)
	chip_types="bcm1125wb"
	install_board_type="cswarm"
	fw_types="diag_vapi_be diag_vapi_le"
	fw_types="$fw_types diag_os_be diag_os_le"
	fw_types="$fw_types general_be general_le"
	;;
bcm91120c)
	chip_types="bcm1120"
	fw_types="diag_os_be diag_os_le"
	fw_types="$fw_types diag_vapi_be diag_vapi_le"
	fw_types="$fw_types general_be general_le"
	;;
bcm91125c)
	chip_types="bcm1125 bcm1125h"
	fw_types="diag_os_be diag_os_le"
	fw_types="$fw_types diag_vapi_be diag_vapi_le"
	fw_types="$fw_types general_be general_le"
	;;
*)
	echo "don't know how to make firmware for $board_type"
	exit 1
	;;
esac

set -e

(cd $board_type && gmake clean)
for fw_type in $fw_types; do

	makeopts=
	case "$fw_type" in
	general_be)
		dir=${board_type}
		makeopts='CFG_LITTLE=0'
		;;
	general_le)
		dir=${board_type}
		makeopts='CFG_LITTLE=1'
		;;
	*)
		dir=${board_type}_${fw_type}
		;;
	esac

	(cd $dir && gmake clean && gmake ${makeopts})

	mkdir -p $commondir
	for chip_type in $chip_types; do
		file=cfe.${chip_type}.${install_board_type}_${fw_type}.srec
		rm -f $commondir/$file
		cp $dir/cfe.srec $commondir/$file
	done

	(cd $dir && gmake clean)
done
