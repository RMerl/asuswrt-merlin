
                      HOW TO ACTIVATE LIBSAMPLERATE 
                      (a.k.a SRC/Secret Rabbit Code)
                AS PJMEDIA'S SAMPLE RATE CONVERSION BACKEND

                                   by
                              Benny Prijono
                                  PJSIP

Background
----------
   Secret Rabbit Code (aka libsamplerate) is a sample rate conversion
   library, available from http://www.mega-nerd.com/SRC/index.html.
   It is licensed under dual license, GPL and proprietary.

   
Supported Platforms
-------------------
   libsamplerate is available for Win32 with Visual Studio and the
   Makefile based targets (such as Linux, MacOS X, *nix, etc.).

   It's not supported for WinCE/Windows Mobile and Symbian since it is
   a floating point based implementation.

   
Installation
------------
   - Download libsamplerate from http://www.mega-nerd.com/SRC/index.html

   - Untar libsamplerate-0.1.2.tar.gz into third_party directory
       cd third_party
       tar xzf libsamplerate-0.1.2.tar.gz

   - Rename libsamplerate-0.1.2 directory name to libsamplerate
       On Windows:
          ren libsamplerate-0.1.2 libsamplerate

       On Linux/Unix/MacOS X:
          mv libsamplerate-0.1.2 libsamplerate


Visual Studio Build
-------------------
   For Visual Studio projects, only static linkage is supported 
   by PJMEDIA build system. If dynamic linking is desired, edit
   pjmedia/src/pjmedia/resample_libresample.c to prevent it from
   linking with the static library, and configure your project
   to link with libsamplerate DLL library.
 
   To build libresample static library with Visual Studio:

      - Open third_party/build/samplerate/libsamplerate_static.dsp
      - Build the project for both Debug and Release build


   libresample dynamic library can be produced by following the
   instructions in libresample source directory.
   

Makefile build
--------------
   - Build and install libsamplerate (configure && make && make install).
     Please follow the instructions in libsamplerate documentation.

   - Re-run PJSIP's "configure" script with this option:

       ./configure --enable-libsamplerate

     this will detect the presence of libsamplerate library and add it
     to the input library list.


Enabling libsamplerate for PJMEDIA's resample
---------------------------------------------
    For both Visual Studio and Makefile based build system, add this in 
    config_site.h:

      #define PJMEDIA_RESAMPLE_IMP PJMEDIA_RESAMPLE_LIBSAMPLERATE


Limitations
-----------
Sample rate 22050 Hz is only supported with 20ms ptime, and sample rate 11025 Hz is only supported with 40ms ptime. This is the limitation of PJMEDIA rather than libsamplerate.


