#!/bin/sh

export LC_ALL=POSIX
export LC_CTYPE=POSIX

prefix=${1}
if [ -z "$prefix" ]; then
	echo "usage: applets/install.sh DESTINATION [--symlinks/--hardlinks/--scriptwrapper]"
	exit 1;
fi
h=`sort busybox.links | uniq`
scriptwrapper="n"
cleanup="0"
noclobber="0"
case "$2" in
	--hardlinks)     linkopts="-f";;
	--symlinks)      linkopts="-fs";;
	--scriptwrapper) scriptwrapper="y";swrapall="y";;
	--sw-sh-hard)    scriptwrapper="y";linkopts="-f";;
	--sw-sh-sym)     scriptwrapper="y";linkopts="-fs";;
	--cleanup)       cleanup="1";;
	--noclobber)     noclobber="1";;
	"")              h="";;
	*)               echo "Unknown install option: $2"; exit 1;;
esac

if [ -n "$DO_INSTALL_LIBS" ] && [ "$DO_INSTALL_LIBS" != "n" ]; then
	# get the target dir for the libs
	# assume it starts with lib
	libdir=$($CC -print-file-name=libc.so | \
		 sed -n 's%^.*\(/lib[^\/]*\)/libc.so%\1%p')
	if test -z "$libdir"; then
		libdir=/lib
	fi

	mkdir -p $prefix/$libdir || exit 1
	for i in $DO_INSTALL_LIBS; do
		rm -f $prefix/$libdir/$i || exit 1
		if [ -f $i ]; then
			cp -pPR $i $prefix/$libdir/ || exit 1
			chmod 0644 $prefix/$libdir/$i || exit 1
		fi
	done
fi

if [ "$cleanup" = "1" ] && [ -e "$prefix/bin/busybox" ]; then
	inode=`ls -i "$prefix/bin/busybox" | awk '{print $1}'`
	sub_shell_it=`
	cd "$prefix"
	for d in usr/sbin usr/bin sbin bin; do
		pd=$PWD
		if [ -d "$d" ]; then
			cd $d
			ls -iL . | grep "^ *$inode" | awk '{print $2}' | env -i xargs rm -f
		fi
		cd "$pd"
	done
	`
	exit 0
fi

rm -f $prefix/bin/busybox || exit 1
mkdir -p $prefix/bin || exit 1
install -m 755 busybox $prefix/bin/busybox || exit 1

for i in $h; do
	appdir=`dirname $i`
	mkdir -p $prefix/$appdir || exit 1
	if [ "$scriptwrapper" = "y" ]; then
		if [ "$swrapall" != "y" ] && [ "$i" = "/bin/sh" ]; then
			ln $linkopts busybox $prefix$i || exit 1
		else
			rm -f $prefix$i
			echo "#!/bin/busybox" > $prefix$i
			chmod +x $prefix/$i
		fi
		echo "	$prefix$i"
	else
		if [ "$2" = "--hardlinks" ]; then
			bb_path="$prefix/bin/busybox"
		else
			case "$appdir" in
			/)
				bb_path="bin/busybox"
			;;
			/bin)
				bb_path="busybox"
			;;
			/sbin)
				bb_path="../bin/busybox"
			;;
			/usr/bin|/usr/sbin)
				bb_path="../../bin/busybox"
			;;
			*)
			echo "Unknown installation directory: $appdir"
			exit 1
			;;
			esac
		fi
		if [ "$noclobber" = "0" ] || [ ! -e "$prefix$i" ]; then
			echo "  $prefix$i -> $bb_path"
			ln $linkopts $bb_path $prefix$i || exit 1
		else
			echo "  $prefix$i already exists"
		fi
	fi
done

exit 0
