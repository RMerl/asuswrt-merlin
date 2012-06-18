
Atari format support
====================

Both mkdosfs and dosfsck now can also handle the Atari variation of
the MS-DOS filesystem format. The Atari format has some minor
differences, some caused by the different machine architecture (m68k),
some being "historic" (Atari didn't change some things that M$
changed).

Both tools automatically select Atari format if they run on an Atari.
Additionally the -A switch toggles between Atari and MS-DOS format.
I.e., on an Atari it selects plain DOS format, on any other machine it
switches to Atari format.

The differences are in detail:

 - Atari TOS doesn't like cluster sizes != 2, so the usual solution
   for bigger partitions was to increase the logical sector size. So
   mkdosfs can handle sector sizes != 512 now, you can also manually
   select it with the -S option. On filesystems larger than approx. 32
   MB, the sector size is automatically increased (stead of the
   cluster size) to make the filesystem fit. mkdosfs will always use 2
   sectors per cluster (also with the floppy standard configurations),
   except when directed otherwise on the command line.

 - From the docs, all values between 0xfff8 and 0xffff in the FAT mark
   an end-of-file. However, DOS usually uses 0xfff8 and Atari 0xffff.
   This seems to be only an consmetic difference. At least TOS doesn't
   complain about 0xffff EOF marks. Don't know what DOS thinks of
   0xfff8 :-) Anyway, both tools use the EOF mark common to the
   system (DOS/Atari).

 - Something similar of the bad cluster marks: On Atari the FAT values
   0xfff0 to 0xfff7 are used for this, under DOS only 0xfff7 (the
   others can be normal cluster numbers, allowing 7 more clusters :-)
   However, both systems usually mark with 0xfff7. Just dosfsck has to
   interpret 0xfff0...0xfff7 differently.

 - Some fields in the boot sector are interpreted differently. For
   example, Atari has a disk serial number (used to aid disk change
   detection) where DOS stores the system name; the 'hidden' field is
   32 bit for DOS, but 16 bit for Atari, and there's no 'total_sect'
   field; the 12/16 bit FAT decision is different: it's not based on
   the number of clusters, but always FAT12 on floppies and FAT16 on
   hard disks. mkdosfs nows about these differences and constructs the
   boot sector accordingly.

 - In dosfsck, the boot sector differences also have to known, to not
   warn about things that are no error on Atari. In addition, most
   Atari formatting tools fill the 'tracks' and 'heads' fields with 0
   for hard disks, because they're meaningless on SCSI disks (Atari
   has/had no IDE). Due to this, the check that they should be
   non-zero is switched off.

 - Under Atari TOS, some other characters are illegal in filenames:
   '<', '>', '|', '"', and ':' are allowed, but all non-ASCII chars
   (codes >= 128) are forbidden.

- Roman <Roman.Hodek@informatik.uni-erlangen.de>
