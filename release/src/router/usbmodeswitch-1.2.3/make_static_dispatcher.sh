#!/bin/sh
# This will customize the jim library for use with usb_modeswitch
# and make the compile result optimized for small size
if [ ! -e "jim/libjim.a" ]; then
	cd jim
	if [ ! -e "autosetup/jimsh0.c" ]; then
		echo "Creating the Jim bootstrap source ..."
		./make-bootstrap-jim >autosetup/jimsh0.c
	fi
	export CFLAGS="-Os"
	echo "Configuring the Jim library ..."
	./configure --disable-lineedit --with-out-jim-ext="stdlib posix load signal syslog" --prefix=/usr
	echo "Compiling the Jim library ..."
	make lib
	cd ..
fi

SHELL=`which tclsh 2>/dev/null`
if [ -z $SHELL ]; then
	SHELL=`which jimsh 2>/dev/null`
fi
if [ -z $SHELL ]; then
	SHELL="jim/autosetup/jimsh0"
	if [ ! -e $SHELL ] ; then
    	gcc -o "jim/autosetup/jimsh0" "jim/autosetup/jimsh0.c"
	fi
	if [ ! -e $SHELL ] ; then
		echo "No Tcl shell found!"
		exit 1
	fi
else
	echo ""
	echo "------"
	echo "Note: found a Tcl shell on your system; embedded interpreter not essential."
	echo "Recommending default installation with \"make install\" ..."
	echo "------"
	echo ""
fi

$SHELL make_string.tcl usb_modeswitch.tcl >usb_modeswitch.string

export CFLAGS="$CFLAGS -Wall -I./jim"
export LDLIBS="$LDLIBS -L./jim -ljim"
echo "Compiling the usb_modeswitch dispatcher ..."
gcc $CFLAGS dispatcher.c $LDLIBS -o usb_modeswitch_dispatcher
strip usb_modeswitch_dispatcher
echo "Done!"

