                                                    -*- mode: indented-text -*-

 - dosfsck: Better checking of file times: ctime <= mtime <= atime

 - mkdosfs: If /etc/bootsect.dos (or similar) exists, use it as a
   template for generating boot sectors. This way, you can, e.g., make
   bootable DOS disks.

   Addendum: Don't know if that's so wise... There are really many
   variants of DOS/Windows bootcode out in the wild, and the code is
   proprietary, too.

 - dosfsck: read-only sector test (-t without -a or -r); just print
   out errors.
