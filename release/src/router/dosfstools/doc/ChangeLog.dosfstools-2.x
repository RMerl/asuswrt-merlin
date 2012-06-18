version 2.11
============

 - all: don't use own llseek() anymore, glibc lseek() does everything we need
 - dosfsck: lfn.c: avoid segfault
 - dosfsck: check.c, lfn.c: check for orphaned LFN slots
 - dosfsck: check.c alloc_rootdir_entry(): set owner of newly alloced clusters
 - dosfsck: dosfsck.h: better use <byteswap.h> for byte swapping
 - dosfsck: io.c: added code for real DOS
 - mkdosfs: raised FAT12_THRESHOLD from 4078 to 4085, introduced MIN_CLUST_32
 - mkdosfs: fix loop device size
 - mkdosfs: by default, use FAT32 on devices >= 512MB
 - mkdosfs: fix a memory leak (blank_sector)
 - mkdosfs: fix parsing of number of blocks on command line, so that numbers
   >2G can be used
 - mkdosfs: add 'b' to getopt() string so this option can be used :)
 - mkdosfs: fix parsing of -i arg (should be unsigned)
 - mkdosfs: change default permissions of created images (-C) to 0666 & ~umask
 - mkdosfs: relax geometry check: if HDIO_GETGEO fails, print a warning and
   default to H=255,S=63
 - dosfsck: new option -n (no-op): just check non-interactively, but
   don't write anything to filesystem
 - A few #include changes to support compilation with linux 2.6
   headers (thanks to Jim Gifford <jim@jg555.com>)
 - dosfsck: remove directory entries pointing to start cluster 0, if they're
   not "." or ".." entries that should actually point to the root dir
   (pointed out by Thomas Winkler <twinkler@sysgo.de>)
 - mkdosfs: new option -h to set number of hidden sectors
   (thanks to Godwin Stewart <gstewart@spamcop.net>)
 - all: updated my mail address everywhere...

version 2.10
============

 - dosfsck: various 64-bit fixes and removed some warnings by Michal
   Cihar <mcihar@suse.cz>
 - mkdosfs: better error message if called without parameters (also
   suggested by Michal)

version 2.9
===========

 - dosfsck: if EOF from stdin, exit with error code
 - dosfsck: Fix potential for "Internal error: next_cluster on bad cluster".
 - dosfsck: When clearing long file names, don't overwrite the dir
   entries with all zeros, but put 0xe5 into the first byte.
   Otherwise, some OSes stop reading the directory at that point...
 - dosfsck: in statistics printed by -v, fix 32bit overflow in number
   of data bytes.
 - dosfsck: fix an potential overflow in "too many clusters" check
 - dosfsck: fix 64bit problem in fat.c (Debian bug #152769)
 - dosfsck: allow FAT size > 32MB.
 - dosfsck: allow for only one FAT
 - dosfsck: with -v, also check that last sector of the filesystem can
   be read (in case a partition is smaller than the fs thinks)
 - mkdosfs: add note in manpage that creating bootable filesystems is
   not supported.
 - mkdosfs: better error message with pointer to -I if target is a
   full-disk device.

version 2.8
===========

 - dosfsck: Fixed endless loop whenever a volume label was present.

version 2.7
===========

 - dosfsck: Don't check volume label for bad characters, everything
   seems to be allowed there... Also ignore duplicate names where one of
   them is a volume label.

version 2.6
===========

 - mkdosfs: Added correct heads definition for 2.88M floppies if not
   created via loopback.
 - dosfsck: If boot sector and its backup are different (FAT32), offer
   to write the backup to sector 0. (tnx to Pavel Roskin for this)
 - For 64 bit alpha, struct bootsector in dosfsck.h must be defined
   with __attribute__((packed)).
 - mkdosfs now actually accepts -R option. (tnx to David Kerrawn)
 - Fixed typo in dosfsck boot.c (recognition of boot signature in FSINFO)
 - Various compilation fixes for 2.4 kernel headers and for ia64.

version 2.5
===========

 - The llseek() implementation for alpha didn't really work; fixed it.

version 2.4
===========

 - Fix compiling problem on alpha (made a silly typo...)

version 2.3
===========

 - mkdosfs: Fixed usage message (printed only "bad address").
 - both: made man pages and usage statements more consistent.
 - both: fix llseek function for alpha.
 - dosfsck: fix reading of unaligned fields in boot sector for alpha.
 - dosfsck: fixed renaming of files (extension wasn't really written).

version 2.2
===========

 - Added dosfsck/COPYING, putting dosfsck officially under GPL (Werner
   and I agree that it should be GPL).
 - mkdosfs: Allow creation of a 16 bit FAT on filesystems that are too
   small for it if the user explicitly selected FAT16 (but a warning
   is printed). Formerly, you got the misleading error message "make
   the fs a bit smaller".
 - dosfsck: new option -y as synonym for -y; for compability with
   other fs checkers, which also accept this option.
 - dosfsck: Now prints messages similar to e2fsck: at start version
   and feature list; at end number of files (and directories) and
   number of used/total clusters. This makes the printouts of *fsck at
   boot time nicer.
 - dosfsck: -a (auto repair) now turns on -f (salvage files), too. -a
   should act as non-destructive as possible, so lost clusters should
   be assigned to files. Otherwise the data in them might be
   overwritten later.
 - dosfsck: Don't drop a directory with lots of bad entries in
   auto-repair mode for the same reason as above.
 - dosfsck: avoid deleting the whole FAT32 root dir if something is
   wrong with it (bad start cluster or the like).
 - general: also create symlinks {mkfs,fsck}.vfat.8 to the respective
   real man pages.

version 2.1
===========

 - Fix some forgotten loff_t's for filesystems > 4GB. (Thanks to
   <ki@kretz.co.at>).
 - Fix typo in mkdosfs manpage.
 - Removed inclusion of <linux/loop.h> from mkdosfs.c; it's unnecessary and
   caused problems in some environments.
 - Fix condition when to expect . and .. entries in a directory. (Was
   wrong for non-FAT32 if first entry in root dir was a directory also.)
 - Also create mkfs.vfat and fsck.vfat symlinks, so that also
   filesystems listed with type "vfat" in /etc/fstab can be
   automatically checked.

version 2.0
===========

 - merge of mkdosfs and dosfstools in one package
 - new maintainer: Roman Hodek <Roman.Hodek@informatik.uni-erlangen.de>
 - FAT32 support in both mkdosfs and dosfsck
 - VFAT (long filename) support in dosfsck
 - Support for Atari variant of MS-DOS filesystem in both tools
 - Working support for big-endian systems in both tools
 - Better support for loop devices in mkdosfs: usual floppy sizes are
   detected and media byte etc. set accordingly; if loop fs has no
   standard floppy size, use hd params
   (mainly by Giuliano Procida <gpp10@cus.cam.ac.uk>)
 - Removed lots of gcc warnings
 - Fixed some minor calculation bugs in mkdosfs.

For change logs previous to 2.0, see the CHANGES files in the subdirectories.
