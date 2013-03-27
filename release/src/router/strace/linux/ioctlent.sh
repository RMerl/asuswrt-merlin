#! /bin/sh
#
# Copyright (c) 2001 Wichert Akkerman <wichert@cistron.nl>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote products
#    derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#	$Id$
#

# Validate arg count.
case $# in
1)
	dir="$1"
	asm=asm
	;;
2)
	dir="$1"
	asm="$2"
	;;
*)
        echo "usage: $0 include-directory [asm-subdirectory]" >&2
        exit 1
	;;
esac

lookup_ioctls()
{
	type="$1"
	shift

	# Build the list of all ioctls
	regexp='^[[:space:]]*#[[:space:]]*define[[:space:]]\+[A-Z][A-Z0-9_]*[[:space:]]\+0x'"$type"'..\>'
	(cd "$dir" && grep "$regexp" "$@" /dev/null 2>/dev/null) |
		sed -ne "s,$asm/,asm/,g"'
s/^\(.*\):[[:space:]]*#[[:space:]]*define[[:space:]]*\([A-Z0-9_]*\)[[:space:]]*\(0x'"$type"'..\).*/	{ "\1",	"\2",	\3	},/p' \
		>> ioctls.h
}


> ioctls.h

lookup_ioctls 03 linux/hdreg.h
lookup_ioctls 22 scsi/sg.h
lookup_ioctls 46 linux/fb.h
lookup_ioctls 4B linux/kd.h
lookup_ioctls 4C linux/loop.h
lookup_ioctls 53 linux/cdrom.h scsi/scsi.h scsi/scsi_ioctl.h
lookup_ioctls 54 $asm/ioctls.h asm-generic/ioctls.h
lookup_ioctls 56 linux/vt.h
lookup_ioctls '7[12]' linux/videotext.h
lookup_ioctls 89 $asm/sockios.h asm-generic/sockios.h linux/sockios.h
lookup_ioctls 8B linux/wireless.h

if [ -e $dir/Kbuild ]; then
	# kernel has exported user space headers, so query only them
	files=$(
		cd $dir || exit
		find . -mindepth 2 -name Kbuild | \
			sed -e 's:^\./::' -e 's:/Kbuild:/*:' | \
			grep -v '^asm-'
		echo "$asm/* asm-generic/*"
	)
	# special case: some headers aren't exported directly
	files="${files} media/* net/bluetooth/* pcmcia/*"
else
	# older kernel so just assume some headers
	files="linux/* $asm/* asm-generic/* scsi/* sound/*"
fi

# Build the list of all ioctls
# Example output:
# { "asm/ioctls.h",	"TIOCSWINSZ",	0x5414  },
# { "asm/mce.h",	"MCE_GETCLEAR_FLAGS",	_IOC(_IOC_NONE,'M',3,0) },
regexp='^[[:space:]]*#[[:space:]]*define[[:space:]]\+[A-Z][A-Z0-9_]*[[:space:]]\+_S\?\(IO\|IOW\|IOR\|IOWR\)\>'
(cd $dir && grep $regexp $files 2>/dev/null) | \
	sed -n \
	-e "s,$asm/,asm/,g" \
	-e 's/^\(.*\):[[:space:]]*#[[:space:]]*define[[:space:]]*\([A-Z0-9_]*\)[[:space:]]*_S\?I.*(\([^[,]*\)[[:space:]]*,[[:space:]]*\([^,)]*\).*/	{ "\1",	"\2",	_IOC(_IOC_NONE,\3,\4,0)	},/p' \
	>> ioctls.h

# Sort and drop dups?
# sort -u <ioctls.h >ioctls1.h && mv ioctls1.h ioctls.h


> ioctldefs.h

# Collect potential ioctl names. ('bases' is a bad name. Sigh)
# Some use a special base to offset their ioctls on. Extract that as well.
# Some use 2 defines: _IOC(_IOC_NONE,DM_IOCTL,DM_LIST_DEVICES_CMD,....)
bases=$(sed -n \
       -e 's/.*_IOC_NONE.*,[[:space:]]*\([A-Z][A-Z0-9_]\+\)[[:space:]]*,[[:space:]]*\([A-Z][A-Z0-9_]\+\)[[:space:]+,].*/\1\n\2/p' \
       -e 's/.*_IOC_NONE.*,[[:space:]]*\([A-Z][A-Z0-9_]\+\)[[:space:]+,].*/\1/p' \
       ioctls.h | sort -u)

for base in $bases; do
	echo "Looking for $base"
	regexp="^[[:space:]]*#[[:space:]]*define[[:space:]]\+$base"
	line=$( (cd $dir && grep -h $regexp 2>/dev/null $files) | grep -v '\<_IO')
	if [ x"$line" != x ]; then
		echo "$base is a #define" # "($line)"
		echo "$line" >> ioctldefs.h
	fi

	if ! grep "\<$base\>" ioctldefs.h >/dev/null 2>/dev/null; then
		# Not all ioctl's are defines ... some (like the DM_* stuff)
		# are enums, so we have to extract that crap ourself
		(
		cd $dir || exit
		# -P: inhibit generation of linemarkers
		${CPP:-cpp} -P $(grep -l $base $files 2>/dev/null) | sed '/^$/d' | \
		awk -v base="$base" '{
			if ($1 == "enum") {
				val = 0
				while ($NF != "};") {
					if (!getline)
						exit
					gsub(/,/, "")
					if ($0 ~ /=/)
						val = $NF
					if ($1 == base) {
						print "#define " base " (" val ")"
						exit
					}
					val++
				}
			}
		}'
		) >> ioctldefs.h
		if ! grep "\<$base\>" ioctldefs.h >/dev/null 2>/dev/null; then
			echo "Can't find the definition for $base"
		else
			echo "$base is an enum"
		fi
	fi
done

# Sort and drop dups?
# sort -u <ioctldefs.h >ioctldefs1.h && mv ioctldefs1.h ioctldefs.h
