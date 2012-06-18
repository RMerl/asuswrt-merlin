#!/usr/bin/sh
PRODUCT=Samba
DEPOT=samba.depot
PSF=samba.psf
export PRODUCT
export DEPOT
export PSF

mkdir codepage >/dev/null 2>&1
mkdir man >/dev/null 2>&1

./gen_psf.sh $PRODUCT

echo "Creating software depot ($DEPOT) for product $PRODUCT"

/usr/sbin/swpackage -vv -s samba.psf -x target_type=tape -d `pwd`/$DEPOT $PRODUCT 

#- clean-up temporary directories
rm -r codepage
rm -r man

