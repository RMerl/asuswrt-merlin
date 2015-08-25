 Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)

 This program is free software: you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software Foundation, either version 2 of the License, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 for more details.

 You should have received a copy of the GNU General Public License along
 with this program. If not, see http://www.gnu.org/licenses/.


Getting Started: Building and Using PJSIP and PJMEDIA

   [Last Update: $Date: 2007-02-02 20:42:44 +0000 (Fri, 02 Feb 2007) $]

                                                   Print Friendly Page
     _________________________________________________________________

   This article describes how to download, customize, build, and use the open
   source PJSIP and PJMEDIA SIP and media stack. The online (and HTML) version
   of this file can be downloaded from http://www.pjsip.org/using.htm


Quick Info
     _________________________________________________________________

   Building with GNU tools (Linux, *BSD, MacOS X, mingw, etc.)
          Generally these should be all that are needed to build the libraries,
          applications, and samples:

   $ ./configure
   $ make dep && make clean && make

   Building Win32 Target with Microsoft Visual Studio
          Generally we can just do these steps:

         1. Visual Studio 6: open pjproject.dsw workspace,
         2. Visual Studio 2005: open pjproject-vs8.sln solution,
         3. Create an empty pjlib/include/pj/config_site.h, and
         4. build the pjsua application.

   Building for Windows Mobile
          Generally these are all that are needed:

         1. Open pjsip-apps/build/wince-evc4/wince_demos.vcw EVC4 workspace,
         2. Create an empty pjlib/include/pj/config_site.h, and
         3. build the pjsua_wince application.

   Invoking Older Build System (e.g. for RTEMS)
          Generally these should be all that are needed to build the libraries,
          applications, and samples:

   $ ./configure-legacy
   $ make dep && make clean && make

   Locating Output Binaries/Libraries
          Libraries will be put in lib directory, and binaries will be put in
          bin directory, under each projects.

   Running the Applications
          After successful build, you can try running pjsua application on
          pjsip-apps/bin   directory.   PJSUA  manual  can  be  found  in
          http://www.pjsip.org/pjsua.htm page.


Table of Contents:
     _________________________________________________________________

   1. Getting the Source Distribution

     1.1 Getting the Release tarball

     1.2 Getting from Subversion trunk

     1.3 Source Directories Layout

   2. Build Preparation

     2.1 config_site.h file

     2.2 Disk Space Requirements

   3.  Building Linux, *nix, *BSD, and MacOS X Targets with GNU Build
   Systems

     3.1 Supported Targets

     3.2 Requirements

     3.3 Running configure

     3.4 Running make

     3.5 Cross Compilation

     3.6 Build Customizations

   4. Building for Windows Targets with Microsoft Visual Studio

     4.1 Requirements

     4.2 Building the Projects

     4.3 Debugging the Sample Application

   5. Building for Windows Mobile Targets (Windows CE/WinCE/PDA/SmartPhone)

     5.1 Requirements

     5.2 Building the Projects

   6. Older PJLIB Build System for Non-Autoconf Targets (e.g. RTEMS)

     6.1 Supported Targets

     6.2 Invoking the Build System

   7. Running the Applications

     7.1 pjsua

     7.2 Sample Applications

     7.3 pjlib-test

     7.4 pjsip-test

   8. Using PJPROJECT with Applications


   Appendix I: Common Problems/Frequently Asked Question (FAQ)

     I.1 fatal error C1083: Cannot open include file: 'pj/config_site.h':
   No such file or directory


1. Getting the Source Code Distribution
     _________________________________________________________________

   All libraries (PJLIB, PJLIB-UTIL, PJSIP, PJMEDIA, and PJMEDIA-CODEC) are
   currently distributed under a single source tree, collectively named as
   PJPROJECT or just PJ libraries. These libraries can be obtained by either
   downloading the release tarball or getting them from the Subversion trunk.


1.1 Getting the Release tarball
     _________________________________________________________________

   Getting the released tarball is a convenient way to obtain stable version of
   PJPROJECT. The tarball may not contain the latest features or bug-fixes, but
   normally it is considered more stable as each will be tested more rigorously
   before released.

   The   latest   released   tarball   can   be   downloaded   from   the
   http://www.pjsip.org/download.htm.


1.2 Getting from Subversion trunk
     _________________________________________________________________

   PJPROJECT  Subversion  repository  will always contain the latest/most
   up-to-date version of the sources. Normally the Subversion repository is
   always kept in a "good" state. However, there's always a chance that things
   break  and  the  tree  doesn't  build  correctly (particularly for the
   "not-so-popular" targets), so please consult the mailing list should there
   be any problems.

   Using Subversion also has benefits of keeping the local copy of the source
   up to date with the main PJ source tree and to easily track the changes made
   to the local copy, if any.


What is Subversion

   Subversion (SVN) is Open Source version control system similar to CVS.
   Subversion homepage is in http://subversion.tigris.org/


Getting Subversion Client

   A Subversion (SVN) client is needed to download the PJ source files from
   pjsip.org  SVN  tree.  SVN  client  binaries  can  be  downloaded from
   http://subversion.tigris.org/, and the program should be available for
   Windows, Linux, MacOS X, and many more platforms.


Getting the Source for The First Time

   Once Subversion client is installed, we can use these commands to initially
   retrieve the latest sources from the Subversion trunk:



   $ svn co http://svn.pjproject.net/repos/pjproject/trunk pjproject
   $ cd pjproject


Keeping The Local Copy Up-to-Date

   Once sources have been downloaded, we can keep the local copy up to date by
   periodically synchronizing the local source with the latest revision from
   the  PJ's  Subversion  trunk. The mailing list provides best source of
   information about the availability of new updates in the trunk.

   To  update  the  local  copy  with the latest changes in the main PJ's
   repository:



   $ cd pjproject
   $ svn update


Tracking Local and Remote Changes

   To see what files have been changed locally:



   $ cd pjproject
   $ svn status

   The above command only compares local file against the original local copy,
   so it doesn't require Internet connection while performing the check.

   To see both what files have been changed locally and what files have been
   updated in the PJ's Subversion repository:



   $ cd pjproject
   $ svn status -u

   Note that this command requires active Internet connection to query the
   status of PJPROJECT's source repository.


1.3 Source Directories Layout
     _________________________________________________________________

Top-Level Directory Layout

   The top-level directories (denoted as $TOP here) in the source distribution
   contains the following sub-directories:

   $TOP/build
          Contains makefiles that are common for all projects.

   $TOP/pjlib
          Contains  header  and  source files of PJLIB. PJLIB is the base
          portability  and  framework  library which is used by all other
          libraries

   $TOP/pjlib-util
          Contains  PJLIB-UTIL  header and source files. PJLIB-UTIL is an
          auxiliary library that contains utility functions such as scanner,
          XML, STUN, MD5 algorithm, getopt() implementation, etc.

   $TOP/pjmedia
          Contains PJMEDIA and PJMEDIA-CODEC header and source files. The
          sources of various codecs (such as GSM, Speex, and iLBC) can be found
          under this directory.

   $TOP/pjsip
          Contains PJSIP header and source files.

   $TOP/pjsip-apps
          Contains source code for PJSUA and various sample applications.


Individual Directory Inside Each Project

   Each library directory further contains these sub-directories:

   bin
          Contains binaries produced by the build process.

   build
          Contains build scripts/makefiles, project files, project workspace,
          etc. to build the project. In particular, it contains one Makefile
          file  to  build the project with GNU build systems, and a *.dsw
          workspace file to build the library with Microsoft Visual Studio 6 or
          later.

   build/output
          The build/output directory contains the object files and other files
          generated by the build process. To support building multiple targets
          with a single source tree, each build target will occupy a different
          subdirectory under this directory.

   build/wince-evc4
          This directory contains the project/workspace files to build Windows
          CE/WinCE version of the project using Microsoft Embedded Visual C++
          4.

   build/wince-evc4/output
          This directory contains the library, executable, and object files
          generated by Windows Mobile build process.

   docs
          Contains Doxygen configuration file (doxygen.cfg) to generate online
          documentation from the source files. The output documentation will be
          put in this directory as well (for example, docs/html directory for
          the HTML files).

          (to generate Doxygen documentation from the source tree, just run
          "doxygen docs/doxygen.cfg" in the individual project directory. The
          generated files will reside in docs directory).

   include
          Contains the header files for the project.

   lib
          Contains libraries produced by the build process.

   src
          Contains the source files of the project.


2. Build Preparation
     _________________________________________________________________

2.1 Create config_site.h file
     _________________________________________________________________

   Before source files can be built, the pjlib/include/pj/config_site.h file
   must be created (it can just be an empty file).

   Note:
          When the Makefile based build system is used, this process is taken
          care by the Makefiles. But when non-Makefile based build system (such
          as Visual Studio) is used, the config_site.h file must be created
          manually.


What is config_site.h File

   The pjlib/include/pj/config_site.h contains local customizations to the
   libraries.

   All customizations should be put in this file instead of modifying PJ's
   files, because if PJ's files get modified, then those modified files will
   not be updated the next time the source is synchronized. Or in other case,
   the local modification may be overwritten with the fresh copy from the SVN.

   Putting the local customization to the config_site.h solves this problem,
   because this file is not included in the version control, so it will never
   be overwritten by "svn update" command.

   Please find list of configuration macros that can be overriden from these
   files:
     * PJLIB Configuration (the pjlib/config.h file)
     * PJLIB-UTIL Configuration (the pjlib-util/config.h file)
     * PJMEDIA Configuration (the pjmedia/config.h file)
     * PJSIP Configuration (the pjsip/sip_config.h file)

   A     sample    config_site.h    file    is    also    available    in
   pjlib/include/config_site_sample.h.


Creating config_site.h file

   The simplest way is just to create an empty file, to use whetever default
   values set by the libraries.

   Another way to create the config_site.h file is to write something like the
   following:


   // Uncomment to get minimum footprint (suitable for 1-2 concurrent calls
   only)
   //#define PJ_CONFIG_MINIMAL_SIZE
   // Uncomment to get maximum performance
   //#define PJ_CONFIG_MAXIMUM_SPEED
   #include <pj/config_site_sample.h>


2.2 Disk Space Requirements
     _________________________________________________________________

   The building process needs:
   about 50-60 MB of disk space to store the uncompressed source files, and
     * about 30-50 MB of additional space for building each target

   (Visual Studio Debug and Release are considered as separate targets)


3. Building Linux, *nix, *BSD, and MacOS X Targets with GNU Build Systems
     _________________________________________________________________

3.1 Supported Targets
     _________________________________________________________________

   The  new,  autoconf  based  GNU  build system can be used to build the
   libraries/applications for the following targets:
     * Linux/uC-Linux (i386, Opteron, Itanium, MIPS, PowerPC, etc.),
     * MacOS X (PowerPC),
     * mingw (i386),
     * FreeBSD and maybe other BSD's (i386, Opteron, etc.),
     * RTEMS with cross compilation (ARM, powerpc),
     * etc.


3.2 Requirements
     _________________________________________________________________

   In order to use PJ's GNU build system, these typical GNU tools are needed:
     * GNU make (other make will not work),
     * GNU binutils for the target, and
     * GNU gcc for the target.
     * OpenSSL header files/libraries (optional) if TLS support is wanted.

   In addition, the appropriate "SDK" must be installed for the particular
   target (this could just be a libc and the appropriate system abstraction
   library such as Posix).

   The build system is known to work on the following hosts:
     * Linux, many types of distributions.
     * MacOS X 10.2
     * mingw (Win2K, XP)
     * FreeBSD (must use gmake instead of make)

   Building Win32 applications with Cygwin is currently not supported by the
   autoconf script (there is some Windows header conflicts), but one can still
   use the old configure script by calling ./configure-legacy. More over,
   cross-compilations might also work with Cygwin.


3.3 Running configure
     _________________________________________________________________

Using Default Settings

   Run  "./configure"  without  any  options to let the script detect the
   appropriate settings for the host:



   $ cd pjproject
   $ ./configure
   ...

   Notes:
          The default settings build the libraries in "release" mode, with
          default CFLAGS set to "-O2 -DNDEBUG". To change the default CFLAGS,
          we can use the usual "./configure CFLAGS='-g'" construct.

    Features Customization

   With the new autoconf based build system, most configuration/customization
   can be specified as configure arguments. The list of customizable features
   can be viewed by running "./configure --help" command:



   $ cd pjproject
   $ ./configure --help
   ...
   Optional Features:
   --disable-floating-point	Disable floating point where possible
   --disable-sound 		Exclude sound (i.e. use null sound)
   --disable-small-filter 	Exclude small filter in resampling
   --disable-large-filter 	Exclude large filter in resampling
   --disable-g711-plc 		Exclude G.711 Annex A PLC
   --disable-speex-aec 		Exclude Speex Acoustic Echo Canceller/AEC
   --disable-g711-codec 	Exclude G.711 codecs from the build
   --disable-l16-codec 		Exclude Linear/L16 codec family from the build
   --disable-gsm-codec 		Exclude GSM codec in the build
   --disable-speex-codec 	Exclude Speex codecs in the build
   --disable-ilbc-codec 	Exclude iLBC codec in the build
   --disable-tls Force excluding TLS support (default is autodetected based on
   OpenSSL availability)
   ...

    Configuring Debug Version and Other Customizations

   The configure script accepts standard customization, which details can be
   obtained by executing ./configure --help.

   Below is an example of specifying CFLAGS in configure:



   $ ./configure CFLAGS="-O3 -DNDEBUG -msoft-float -fno-builtin"
   ...

    Configuring TLS Support

   By default, TLS support is configured based on the availability of OpenSSL
   header files and libraries. If OpenSSL is available at the default include
   and library path locations, TLS will be enabled by the configure script.

   You  can explicitly disable TLS support by giving the configure script
   --disable-tls option.


  3.4 Cross Compilation
     _________________________________________________________________

   Cross compilation should be supported, using the usual autoconf syntax:



   $ ./configure --host=arm-elf-linux
   ...

   Since cross-compilation is not tested as often as the "normal" build, please
   watch for the ./configure output for incorrect settings (well ideally this
   should be done for normal build too).

   Please refer to Porting Guide for further information about porting PJ
   software.


  3.5 Running make
     _________________________________________________________________

   Once the configure script completes successfully, start the build process by
   invoking these commands:



   $ cd pjproject
   $ make dep
   $ make

   Note:
          gmake may need to be specified instead of make for some hosts, to
          invoke GNU make instead of the native make.


   Description of all make targets supported by the Makefile's:

   all
          The default (or first) target to build the libraries/binaries.

   dep, depend
          Build dependencies rule from the source files.

   clean
          Clean  the object files for current target, but keep the output
          library/binary files intact.

   distclean, realclean
          Remove  all  generated  files (object, libraries, binaries, and
          dependency files) for current target.


   Note:
          make can be invoked either in the top-level PJ directory or in build
          directory under each project to build only the particular project.


  3.6 Build Customizations
     _________________________________________________________________

   Build features can be customized by specifying the options when running
   ./configure as described in Running Configure above.

   In addition, additional CFLAGS and LDFLAGS options can be put in user.mak
   file in PJ root directory (this file may need to be created if it doesn't
   exist). Below is a sample of user.mak file contents:



   export CFLAGS += -msoft-float -fno-builtin
   export LDFLAGS +=


4. Building for Windows Targets with Microsoft Visual Studio
     _________________________________________________________________

  4.1 Requirements
     _________________________________________________________________

   The Microsoft Visual Studio based project files can be used with one of the
   following:

     * Microsoft Visual Studio 6,
     * Microsoft Visual Studio .NET 2002,
     * Microsoft Visual Studio .NET 2003,
     * Microsoft Visual C++ 2005 (including Express edition),

   In addition, the following SDK's are needed:
     * Platform SDK, if you're using Visual Studio 2005 Express (tested with
       Platform SDK for Windows Server 2003 SP1),
     * DirectX SDK (tested with DirectX version 8 and 9),
     * OpenSSL development kit would be needed if TLS support is wanted, or
       otherwise this is optional.

   For the host, the following are required:
     * Windows NT, 2000, XP, 2003, or later ,
     * Windows 95/98 should work too, but this has not been tested,
     * Sufficient amount of RAM for the build process (at least 256MB).


    Enabling TLS Support with OpenSSL

   If  TLS  support  is wanted, then OpenSSL SDK must be installed in the
   development host.

   To install OpenSSL SDK from the Win32 binary distribution:
    1. Install OpenSSL SDK to any folder (e.g. C:\OpenSSL)
    2. Add OpenSSL DLL location to the system PATH.
    3. Add OpenSSL include path to Visual Studio includes search directory.
       Make sure that OpenSSL header files can be accessed from the program
       with #include <openssl/ssl.h> construct.
    4. Add OpenSSL library path to Visual Studio library search directory. Make
       sure the following libraries are accessible:
          + For Debug build: libeay32MTd and ssleay32MTd.
          + For Release build: libeay32MT and ssleay32MT.

   Then to enable TLS transport support in PJSIP, just add

     #define PJSIP_HAS_TLS_TRANSPORT 1

   in your pj/config_site.h. When this macro is defined, OpenSSL libraries will
   be automatically linked to the application via the #pragma construct in
   sip_transport_tls_ossl.c file.


  4.2 Building the Projects
     _________________________________________________________________

   Follow the steps below to build the libraries/application using Visual
   Studio:
    1. For Visual Studio 6: open pjproject.dsw workspace file.
    2. For Visual Studio 8 (VS 2005): open pjproject-vs8.sln solution file.
    3. Set pjsua as Active Project.
    4. Select Debug or Release build as appropriate.
    5. Build the project. This will build pjsua application and all libraries
       needed by pjsua.
    6. After  successful  build,  the pjsua application will be placed in
       pjsip-apps/bin directory, and the libraries in lib directory under each
       projects.

   To build the samples:
    1. (Still using the same workspace)
    2. Set samples project as Active Project
    3. Select Debug or Release build as appropriate.
    4. Build the project. This will build all sample applications and all
       libraries needed.
    5. After  successful build, the sample applications will be placed in
       pjsip-apps/bin/samples directory, and the libraries in lib directory
       under each projects.

  4.3 Debugging the Sample Application
     _________________________________________________________________

   The sample applications are build using Samples.mak makefile, therefore it
   is  difficult  to  setup  debugging session in Visual Studio for these
   applications. To solve this issue, the pjsip_apps workspace contain one
   project  called  sample_debug  which  can  be used to debug the sample
   application.

   To setup debugging using sample_debug project:
    1. (Still using pjsip_apps workspace)
    2. Set sample_debug project as Active Project
    3. Edit debug.c file inside this project.
    4. Modify the #include line to include the particular sample application to
       debug
    5. Select Debug build.
    6. Build and debug the project.


5. Building for Windows Mobile Targets (Windows CE/WinCE/PDA/SmartPhone)
     _________________________________________________________________

   PJ supports building SIP and media stacks and applications for Windows
   Mobile targets. A very simple WinCE SIP user agent (with media) application
   is provided just as proof of concept that the port works.

  5.1 Requirements
     _________________________________________________________________

   One of the following development tools is needed to build SIP and media
   components for Windows Mobile:
     * Microsoft Embedded Visual C++ 4 with appropriate SDKs, or
     * Microsoft Visual Studio 2005 for Windows Mobile with appropriate SDKs.

   Note that VS2005 is not directly supported (as I don't have the tools), but
   it is reported to work (I assumed that VS2005 for Windows Mobile can import
   EVC4 workspace file).

  5.2 Building the Projects
     _________________________________________________________________

   The Windows Mobile port is included in the main source distribution. Please
   follow  the  following  steps  to build the WinCE libraries and sample
   application:
    1. Open pjsip-apps/build/wince-evc4/wince_demos.vcw workspace file. If
       later version of EVC4 is being used, this may cause the workspace file
       to be converted to the appropriate format.
    2. Select pjsua_wince project as the Active Project.
    3. Select the appropriate SDK (for example Pocket PC 2003 SDK or SmartPhone
       2003 SDK)
    4. Select the appropriate configuration (for example, Win32 (WCE Emulator
       Debug) to debug the program in emulator, or other configurations such as
       ARMV4, MIPS, SH3, SH4, or whatever suitable for the device)
    5. Select the appropriate device (Emulator or the actual Device).
    6. Build the project. This will build the sample WinCE application and all
       libraries (SIP, Media, etc.) needed by this application.

   Notes

          + If the config_site.h includes config_site_sample.h file, then
            there are certain configuration in config_site_sample.h that get
            activated for Windows CE targets. Please make sure that these
            configurations are suitable for the application.
          + The libraries, binaries and object files produced by the build
            process are located under build/wince-evc4/output directory of each
            projects.


6. Older PJLIB Build System for Non-Autoconf Targets (e.g. RTEMS)
     _________________________________________________________________

   The old PJLIB build system can still be used for building PJ libraries, for
   example for RTEMS target. Please see the Porting PJLIB page in PJLIB
   Reference documentation for information on how to support new target using
   this build system.

  6.1 Supported Targets
     _________________________________________________________________

   The older build system supports building PJ libraries for the following
   operating systems:
     * RTEMS
     * Linux
     * MacOS X
     * Cygwin and Mingw

   And it supports the following target architectures:
     * i386, x86_64, itanium
     * ARM
     * mips
     * powerpc
     * mpc860
     * etc.

   For other targets, specific files need to be added to the build system,
   please see the Porting PJLIB page in PJLIB Reference documentation for
   details.

  6.2 Invoking the Build System
     _________________________________________________________________

   To invoke the older build system, run the following:



   $ cd pjproject
   $ ./configure-legacy
   $ make dep && make clean && make



7. Running the Applications
     _________________________________________________________________

   Upon successful build, the output libraries (PJLIB, PJLIB-UTIL, PJMEDIA,
   PJSIP, etc.) are put under ./lib sub-directory under each project directory.
   In addition, some applications may also be built, and such applications will
   be put in ./bin sub-directory under each project directory.


  7.1 pjsua
     _________________________________________________________________

   pjsua is the reference implementation for both PJSIP and PJMEDIA stack, and
   is  the  main target of the build system. Upon successful build, pjsua
   application will be put in pjsip-apps/bin directory.

   pjsua manual can be found in pjsua Manual Page.


  7.2 Sample Applications
     _________________________________________________________________

   Sample applications will be built with the Makefile build system. For Visual
   Studio, you have to build the samples manually by selecting and building the
   Samples project inside pjsip-apps/build/pjsip_apps.dsw project workspace.

   Upon   successful   build,   the   sample   applications  are  put  in
   pjsip-apps/bin/samples directory.

   The  sample applications are described in PJMEDIA Samples Page and
   PJSIP Samples Page in the website.


  7.3 pjlib-test
     _________________________________________________________________

   pjlib-test contains comprehensive tests for testing PJLIB functionality.
   This application will only be built when the Makefile build system is used;
   with  Visual  Studio, one has to open pjlib.dsw project in pjlib/build
   directory to build this application.

   If  you're  porting PJLIB to new target, it is recommended to run this
   application to make sure that all functionalities works as expected.


  7.4 pjsip-test
     _________________________________________________________________

   pjsip-test contains codes for testing various SIP functionalities in PJSIP
   and also to benchmark static performance metrics such as message parsing per
   second.



8. Using PJPROJECT with Applications
     _________________________________________________________________

   Regardless of the build system being used, the following tasks are normally
   needed to be done in order to build application to use PJSIP and PJMEDIA:
    1. Put these include directories in the include search path:
          + pjlib/include
          + pjlib-util/include
          + pjmedia/include
          + pjsip/include
    2. Put these library directories in the library search path:
          + pjlib/lib
          + pjlib-util/lib
          + pjmedia/lib
          + pjsip/lib
    3. Include the relevant PJ header files in the application source file. For
       example, using these would include ALL APIs exported by PJ:

      #include <pjlib.h>
      #include <pjlib-util.h>
      #include <pjsip.h>
      #include <pjsip_ua.h>
      #include <pjsip_simple.h>
      #include <pjsua.h>
      #include <pjmedia.h>
      #include <pjmedia-codec.h>
       (Note: the documentation of the relevant libraries should say which
       header files should be included to get the declaration of the APIs).
    4. Declare the OS macros.
          + For Windows applications built with Visual Studio, we need to
            declare PJ_WIN32=1 macro in the project settings (declaring the
            macro in the source file may not be sufficient).
          + For Windows Mobile applications build with Visual C++, we need to
            declare PJ_WIN32_WINCE=1 macro in the project settings.
          + For  GNU build system/autoconf based build system, we need to
            declare PJ_AUTOCONF=1 macro when compiling the applications.
       (Note: the old PJ build system requires declaring the target processor
       with PJ_M_XXX=1 macro, but this has been made obsolete. The target
       processor  will  be  detected  from compiler's predefined macro by
       pjlib/config.h file).
    5. Link with the appropriate PJ libraries. The following libraries will
       need to be included in the library link specifications:

        pjlib
                Base library used by all libraries.

        pjlib-util
                Auxiliary library containing scanner, XML, STUN, MD5, getopt,
                etc, used by the SIP and media stack.

        pjsip
                SIP core stack library.

        pjsip-ua
                SIP user agent library containing INVITE session, call
                transfer, client registration, etc.

        pjsip-simple
                SIP SIMPLE library for base event framework, presence, instant
                messaging, etc.

        pjsua
                High level SIP UA library, combining SIP and media stack into
                high-level easy to use API.

        pjmedia
                The media framework.

        pjmedia-codec
                Container library for various codecs such as GSM, Speex, and
                iLBC.


   Note: the actual library names will be appended with the target name and the
   build configuration. For example:

        For Visual Studio builds
                The actual library names will look like
                pjlib-i386-win32-vc6-debug.lib,
                pjlib-i386-win32-vc6-release.lib, etc., depending on whether we
                are building the Debug or Release version of the library.

                An easier way to link with the libraries is to include PJ
                project files in the workspace, and to configure project
                dependencies so that the application depends on the PJ
                libraries. This way, we don't need to manually add each PJ
                libraries to the input library file specification, since VS
                will automatically link the dependency libraries with the
                application.

        For Windows Mobile builds
                Unfortunately the PJ libraries built for Windows Mobile will
                not be placed in the usual lib directory, but rather under the
                output directory under build/wince-evc4 project directory.

                An easier way to link with the libraries is to include PJ
                project files in the workspace, and to configure project
                dependencies so that the application depends on the PJ
                libraries. This way, we don't need to manually add each PJ
                libraries to the input library file specification, since VS
                will automatically link the dependency libraries with the
                application.

        For GNU builds
                Application's Makefile can get the PJ library suffix by
                including PJ's build.mak file from the root PJ directory (the
                suffix is contained in TARGET_NAME variable). For example, to
                link with PJLIB and PJMEDIA, we can use this syntax in the
                LDFLAGS: "-lpj-$(TARGET_NAME) -lpjmedia-$(TARGET_NAME)"


    6. Link with system spesific libraries:

        Windows
                Add (among other things): wsock32.lib, ws2_32.lib, ole32.lib,
                dsound.lib

        Linux, *nix, *BSD
                Add (among other things): '-lpthread -lm' (at least).

        MacOS X
                Add (among other things): '-framework CoreAudio -lpthread -lm'.


Appendix I: Common Problems/Frequently Asked Question (FAQ)
     _________________________________________________________________

  I.1 fatal error C1083: Cannot open include file: 'pj/config_site.h': No such
  file or directory

   This error normally occurs when the config_site.h file has not been created.
   This file needs to be created manually (an empty file is sufficient). Please
   follow the Build Preparation instructions above to create this file.








     _________________________________________________________________

   Feedback:
          Thanks for using PJ libraries and for reading this document. Please
          send feedbacks or general comments to <bennylp at pjsip dot org>.

