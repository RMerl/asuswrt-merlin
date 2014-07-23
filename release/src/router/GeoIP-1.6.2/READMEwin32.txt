=======================================================

Environmental variables:

1. GeoIPDBFileName is hardcoded to "\\windows\\system32\\GeoIP.dat" on
windows in GeoIP.c
2. #ifdef DLL is used to determine whether you want to have a DLL built
in GeoIP.h

You may want to change these depending on your system configuration
and compiler.

=======================================================
Thanks to Chris Gibbs for supplying these instructions.

The GeoIP C library should work under windows.  Note that it requires the zlib
DLL.

To install zlib with GeoIP:

i) Downloda the zlib prebuilt DLL and static library from
http://www.winimage.com/zLibDll/ look for "pre-built zlib DLL".

Unzip it to some location on your hard drive, and in Project-->Settings ,
go to the Link tab, and add the following 3 libraries:

ws2_32.lib
zlib.lib
zlibstat.lib

iii) Go to Tools-->Options, then the Directories tab, and add library paths to
the locations of the zlib static libraries. You will also need to add the
include path to zlib.h to the include paths.

iv) NOTE: These instructions are for MS VC++ 6.0, but should be similar for
previous versions, and for VC .NET.

=======================================================
Building GeoIP as a DLL

Stanislaw Pusep has contributed a patch for building GeoIP as a DLL.
You can find the patch in GeoIPWinDLL.patch

Note a modified version of this patch is now merged into the main code.
