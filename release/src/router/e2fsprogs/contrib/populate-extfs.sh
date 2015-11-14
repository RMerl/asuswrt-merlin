#!/bin/sh
#
# This script uses debugfs command to populate the ext2/3/4 filesystem
# from a given directory.
#

do_usage () {
	cat << _EOF
Usage: populate-extfs.sh <source> <device>
Create an ext2/ext3/ext4 filesystem from a directory or file

  source: The source directory or file
  device: The target device

_EOF
	exit 1
}

[ $# -ne 2 ] && do_usage

SRCDIR=${1%%/}
DEVICE=$2

# Find where is the debugfs command if not found in the env.
if [ -z "$DEBUGFS" ]; then
	CONTRIB_DIR=$(dirname $(readlink -f $0))
	DEBUGFS="$CONTRIB_DIR/../debugfs/debugfs"
fi

{
	CWD="/"
	find $SRCDIR | while read FILE; do
                TGT="${FILE##*/}"
                DIR="${FILE#$SRCDIR}"
                DIR="${DIR%$TGT}"

		# Skip the root dir
		[ ! -z "$DIR" ] || continue
		[ ! -z "$TGT" ] || continue

		if [ "$DIR" != "$CWD" ]; then
			echo "cd $DIR"
			CWD="$DIR"
		fi

		# Only stat once since stat is a time consuming command
		STAT=$(stat -c "TYPE=\"%F\";DEVNO=\"0x%t 0x%T\";MODE=\"%f\";U=\"%u\";G=\"%g\"" $FILE)
		eval $STAT

		case $TYPE in
		"directory")
			echo "mkdir $TGT"
			;;
		"regular file" | "regular empty file")
			echo "write $FILE $TGT"
			;;
		"symbolic link")
			LINK_TGT=$(readlink $FILE)
			echo "symlink $TGT $LINK_TGT"
			;;
		"block special file")
			echo "mknod $TGT b $DEVNO"
			;;
		"character special file")
			echo "mknod $TGT c $DEVNO"
			;;
		"fifo")
			echo "mknod $TGT p"
			;;
		*)
			echo "Unknown/unhandled file type '$TYPE' file: $FILE" 1>&2
			;;
		esac

		# Set the file mode
		echo "sif $TGT mode 0x$MODE"

		# Set uid and gid
		echo "sif $TGT uid $U"
		echo "sif $TGT gid $G"
	done

	# Handle the hard links.
	# Save the hard links to a file, use the inode number as the filename, for example:
	# If a and b's inode number is 6775928, save a and b to /tmp/tmp.VrCwHh5gdt/6775928.
	INODE_DIR=`mktemp -d` || exit 1
	for i in `find $SRCDIR -type f -links +1 -printf 'INODE=%i###FN=%p\n'`; do
		eval `echo $i | sed 's$###$ $'`
		echo ${FN#$SRCDIR} >>$INODE_DIR/$INODE
	done
	# Use the debugfs' ln and "sif links_count" to handle them.
	for i in `ls $INODE_DIR`; do
		# The link source
		SRC=`head -1 $INODE_DIR/$i`
		# Remove the files and link them again except the first one
		for TGT in `sed -n -e '1!p' $INODE_DIR/$i`; do
			echo "rm $TGT"
			echo "ln $SRC $TGT"
		done
		LN_CNT=`cat $INODE_DIR/$i | wc -l`
		# Set the links count
		echo "sif $SRC links_count $LN_CNT"
	done
	rm -fr $INODE_DIR
} | $DEBUGFS -w -f - $DEVICE
