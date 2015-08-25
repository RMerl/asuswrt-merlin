/* $Id: doxygen.h 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */
#ifndef __PJ_DOXYGEN_H__
#define __PJ_DOXYGEN_H__

/**
 * @file doxygen.h
 * @brief Doxygen's mainpage.
 */

/*////////////////////////////////////////////////////////////////////////// */
/*
	INTRODUCTION PAGE
 */

/**
 * @mainpage Welcome to PJLIB!
 *
 * @section intro_sec What is PJLIB
 *
 * PJLIB is an Open Source, small footprint framework library written in C for 
 * making scalable applications. Because of its small footprint, it can be used
 * in embedded applications (we hope so!), but yet the library is also aimed for
 * facilitating the creation of high performance protocol stacks.
 *
 * PJLIB is released under GPL terms.
 *
 * @section download_sec Download
 *
 * PJLIB and all documentation can be downloaded from 
 * http://www.pjsip.org.
 *
 *
 * @section how_to_use_sec About This Documentation
 *
 * This document is generated directly from PJLIB source file using
 * \a doxygen (http://www.doxygen.org). Doxygen is a great (and free!) 
 * tools for generating such documentation.
 *
 *
 * @subsection find_samples_subsec How to Read This Document
 *
 * This documentation is laid out more to be a reference guide instead
 * of tutorial, therefore first time users may find it difficult to
 * grasp PJLIB by reading this document alone.
 *
 * However, we've tried our best to make this document easy to follow.
 * For first time users, we would suggest that you follow these steps
 * when reading this documentation:
 *
 *  - continue reading this introduction chapter. At the end of this
 *    chapter, you'll find section called \ref pjlib_fundamentals_sec
 *    which should guide you to understand basic things about PJLIB.
 *
 *  - find information about specific features that you want to use
 *    in PJLIB. Use the <b>Module Index</b> to find out about all 
 *    features in PJLIB (if you're browsing the HTML documentation,
 *    click on the \a Module link on top of the page, or if you're
 *    reading the PDF documentation, click on \a Module \a Documentation
 *    on the navigation pane on the left).
 *
 * @subsection doc_organize_sec How To's
 *
 * Please find below links to specific tasks that you probably
 * want to do:
 *
 *  - <b>How to Build PJLIB</b>
 *\n
 * Please refer to \ref pjlib_build_sys_pg page for more information.
 *
 *  - <b>How to Use PJLIB in My Application</b>
 *\n
 * Please refer to \ref configure_app_sec for more information.
 *
 *  - <b>How to Port PJLIB</b>
 *\n
 * Please refer to \ref porting_pjlib_pg page.
 *
 *  - <b>Where to Read Samples Documentation</b>
 *\n
 * Most of the modules provide link to the corresponding sample file.
 * Alternatively, to get the list of all examples, you can click on 
 * <b>Related Pages</b> on the top of HTML document or on 
 * <b>PJLIB Page Documentation</b> on navigation pane of your PDF reader.
 *
 *  - <b>How to Submit Code to PJLIB Project</b>
 *\n
 * Please read \ref pjlib_coding_convention_page before submitting
 * your code. Send your code as patch against current Subversion tree
 * to the appropriate mailing list.
 *
 *
 * @section features_sec Features
 *
 * @subsection open_source_feat It's Open Source!
 *
 * PJLIB is currently released on GPL license, but other arrangements
 * can be made with the author.
 *
 * @subsection extreme_portable_feat Extreme Portability
 *
 * PJLIB is designed to be extremely portable. It can run on any kind
 * of processors (16-bit, 32-bit, or 64-bit, big or little endian, single
 * or multi-processors) and operating systems. Floating point or no
 * floating point. Multi-threading or not.
 * It can even run in environment where no ANSI LIBC is available. 
 *
 * Currently PJLIB is known to run on these platforms:
 *  - Win32/x86 (Win95/98/ME, NT/2000/XP/2003, mingw).
 *  - arm, WinCE and Windows Mobile.
 *  - Linux/x86, (user mode and as <b>kernel module</b>(!)).
 *  - Linux/alpha
 *  - Solaris/ultra.
 *  - MacOS X/powerpc
 *  - RTEMS (x86 and powerpc).
 *
 * And efforts is under way to port PJLIB on:
 *  - Symbian OS
 *
 *
 * @subsection small_size_feat Small in Size
 *
 * One of the primary objectives is to have library that is small in size for
 * typical embedded applications. As a rough guidance, we aim to keep the 
 * library size below 100KB for it to be considered as small.
 * As the result, most of the functionalities in the library can be tailored
 * to meet the requirements; user can enable/disable specific functionalities
 * to get the desired size/performance/functionality balance.
 *
 * For more info, please see @ref pj_config.
 *
 *
 * @subsection big_perform_feat Big in Performance
 *
 * Almost everything in PJLIB is designed to achieve the highest possible
 * performance out of the target platform. 
 *
 *
 * @subsection no_dyn_mem No Dynamic Memory Allocations
 *
 * The central idea of PJLIB is that for applications to run as fast as it can,
 * it should not use \a malloc() at all, but instead should get the memory 
 * from a preallocated storage pool. There are few things that can be 
 * optimized with this approach:
 *
 *  - \a alloc() is a O(1) operation.
 *  - no mutex is used inside alloc(). It is assumed that synchronization 
 *    will be used in higher abstraction by application anyway.
 *  - no \a free() is required. All chunks will be deleted when the pool is 
 *    destroyed.
 *
 * The performance gained on some systems can be as high as 30x speed up
 * against \a malloc() and \a free() on certain configurations, but of
 * course your mileage may vary. 
 *
 * For more information, see \ref PJ_POOL_GROUP
 *
 * 
 * @subsection os_abstract_feat Operating System Abstraction
 *
 * PJLIB has abstractions for features that are normally not portable 
 * across operating systems: 
 *  - @ref PJ_THREAD
 *\n
 *    Portable thread manipulation.
 *  - @ref PJ_TLS
 *\n
 *    Storing data in thread's private data.
 *  - @ref PJ_MUTEX
 *\n
 *    Mutual exclusion protection.
 *  - @ref PJ_SEM
 *\n
 *    Semaphores.
 *  - @ref PJ_ATOMIC
 *\n
 *    Atomic variables and their operations.
 *  - @ref PJ_CRIT_SEC
 *\n
 *    Fast locking of critical sections.
 *  - @ref PJ_LOCK
 *\n
 *    High level abstraction for lock objects.
 *  - @ref PJ_EVENT
 *\n
 *    Event object.
 *  - @ref PJ_TIME
 *\n
 *    Portable time manipulation.
 *  - @ref PJ_TIMESTAMP
 *\n
 *    High resolution time value.
 *  - etc.
 *
 *
 * @subsection ll_network_io_sec Low-Level Network I/O
 *
 * PJLIB has very portable abstraction and fairly complete set of API for
 * doing network I/O communications. At the lowest level, PJLIB provides:
 *
 *  - @ref PJ_SOCK
 *\n
 *    A highly portable socket abstraction, runs on all kind of
 *    network APIs such as standard BSD socket, Windows socket, Linux
 *    \b kernel socket, PalmOS networking API, etc.
 *
 *  - @ref pj_addr_resolve
 *\n
 *    Portable address resolution, which implements #pj_gethostbyname().
 *
 *  - @ref PJ_SOCK_SELECT
 *\n
 *    A portable \a select() like API (#pj_sock_select()) which can be
 *    implemented with various back-end.
 *
 *
 *
 * @subsection timer_mgmt_sec Timer Management
 *
 * A passive framework for managing timer, see @ref PJ_TIMER for more info.
 * There is also function to retrieve high resolution timestamp
 * from the system (see @ref PJ_TIMESTAMP).
 *
 *
 * @subsection data_struct_sec Various Data Structures
 *
 * Various data structures are provided in the library:
 *
 *  - @ref PJ_PSTR
 *  - @ref PJ_ARRAY
 *  - @ref PJ_HASH
 *  - @ref PJ_LIST
 *  - @ref PJ_RBTREE
 *
 *
 * @subsection exception_sec Exception Construct
 *
 * A convenient TRY/CATCH like construct to propagate errors, which by
 * default are used by the @ref PJ_POOL_GROUP "memory pool" and 
 * the lexical scanner in pjlib-util. The exception
 * construct can be used to write programs like below:
 *
 * <pre>
 *    #define SYNTAX_ERROR  1
 *
 *    PJ_TRY {
 *       msg = NULL;
 *       msg = parse_msg(buf, len);
 *    }
 *    PJ_CATCH ( SYNTAX_ERROR ) {
 *       .. handle error ..
 *    }
 *    PJ_END;
 * </pre>
 *
 * Please see @ref PJ_EXCEPT for more information.
 *
 *
 * @subsection logging_sec Logging Facility
 *
 * PJLIB @ref PJ_LOG consists of macros to write logging information to
 * some output device. Some of the features of the logging facility:
 *
 *  - the verbosity can be fine-tuned both at compile time (to control
 *    the library size) or run-time (to control the verbosity of the
 *    information).
 *  - output device is configurable (e.g. stdout, printk, file, etc.)
 *  - log decoration is configurable.
 *
 * See @ref PJ_LOG for more information.
 *
 *
 * @subsection guid_gen_sec Random and GUID Generation
 *
 * PJLIB provides facility to create random string 
 * (#pj_create_random_string()) or globally unique identifier
 * (see @ref PJ_GUID).
 *
 *
 *
 * @section configure_app_sec Configuring Application to use PJLIB
 *
 * @subsection pjlib_compil_sec Building PJLIB
 *
 * Follow the instructions in \ref pjlib_build_sys_pg to build
 * PJLIB.
 *
 * @subsection pjlib_compil_app_sec Building Applications with PJLIB
 *
 * Use the following settings when building applications with PJLIB.
 *
 * @subsubsection compil_inc_dir_sec Include Search Path
 *
 * Add this to your include search path ($PJLIB is PJLIB root directory):
 * <pre>
 *   $PJLIB/include
 * </pre>
 *
 * @subsubsection compil_inc_file_sec Include PJLIB Header
 *
 * To include all PJLIB headers:
 * \verbatim
    #include <pjlib.h>
   \endverbatim
 *
 * Alternatively, you can include individual PJLIB headers like this:
 * \verbatim
     #include <pj/log.h>
     #include <pj/os.h>
  \endverbatim
 *
 *
 * @subsubsection compil_lib_dir_sec Library Path
 *
 * Add this to your library search path:
 * <pre>
 *   $PJLIB/lib
 * </pre>
 *
 * Then add the appropriate PJLIB library to your link specification. For
 * example, you would add \c libpj-i386-linux-gcc.a when you're building
 * applications in Linux.
 *
 *
 * @subsection pjlib_fundamentals_sec Principles in Using PJLIB
 *
 * Few things that you \b MUST do when using PJLIB, to make sure that
 * you create trully portable applications.
 *
 * @subsubsection call_pjlib_init_sec Call pj_init()
 *
 * Before you do anything else, call \c pj_init(). This would make sure that
 * PJLIB system is properly set up.
 *
 * @subsubsection no_ansi_subsec Do NOT Use ANSI C
 *
 * Contrary to popular teaching, ANSI C (and LIBC) is not the most portable
 * library in the world, nor it's the most ubiquitous. For example, LIBC
 * is not available in Linux kernel. Also normally LIBC will be excluded
 * from compilation of RTOSes to reduce size.
 *
 * So for maximum portability, do NOT use ANSI C. Do not even try to include
 * any other header files outside <include/pj>. Stick with the functionalities
 * provided by PJLIB. 
 *
 *
 * @subsubsection string_rep_subsubsec Use pj_str_t instead of C Strings
 *
 * PJLIB uses pj_str_t instead of normal C strings. You SHOULD follow this
 * convention too. Remember, ANSI string-h is not always available. And
 * PJLIB string is faster!
 *
 * @subsubsection mem_alloc_subsubsec Use Pool for Memory Allocations
 *
 * You MUST NOT use \a malloc() or any other memory allocation functions.
 * Use PJLIB @ref PJ_POOL_GROUP instead! It's faster and most portable.
 *
 * @subsection logging_subsubsec Use Logging for Text Display
 *
 * DO NOT use <stdio.h> for text output. Use PJLIB @ref PJ_LOG instead.
 *
 *
 * @section porting_pjlib_sec0 Porting PJLIB
 *
 * Please see \ref porting_pjlib_pg page on more information to port
 * PJLIB to new target.
 *
 * @section enjoy_sec Enjoy Using PJLIB!
 *
 * We hope that you find PJLIB usefull for your application. If you
 * have any questions, suggestions, critics, bug fixes, or anything
 * else, we would be happy to hear it.
 *
 * Enjoy using PJLIB!
 *
 * Benny Prijono < bennylp at pjsip dot org >
 */



/*////////////////////////////////////////////////////////////////////////// */
/*
         CODING CONVENTION
 */

/**
 * @page pjlib_coding_convention_page Coding Convention
 *
 * Before you submit your code/patches to be included with PJLIB, you must
 * make sure that your code is compliant with PJLIB coding convention.
 * <b>This is very important!</b> Otherwise we would not accept your code.
 *
 * @section coding_conv_editor_sec Editor Settings
 *
 * The single most important thing in the whole coding convention is editor 
 * settings. It's more important than the correctness of your code (bugs will
 * only crash the system, but incorrect tab size is mental!).
 *
 * Kindly set your editor as follows:
 *  - tab size to \b 8.
 *  - indentation to \b 4.
 *
 * With \c vi, you can do it with:
 * <pre>
 *  :se ts=8
 *  :se sts=4
 * </pre>
 *
 * You should replace tab with eight spaces.
 *
 * @section coding_conv_detail_sec Coding Style
 *
 * Coding style MUST strictly follow K&R style. The rest of coding style
 * must follow current style. You SHOULD be able to observe the style
 * currently used by PJLIB from PJLIB sources, and apply the style to your 
 * code. If you're not able to do simple thing like to observe PJLIB
 * coding style from the sources, then logic dictates that your ability to
 * observe more difficult area in PJLIB such as memory allocation strategy, 
 * concurrency, etc is questionable.
 *
 * @section coding_conv_comment_sec Commenting Your Code
 *
 * Public API (e.g. in header files) MUST have doxygen compliant comments.
 *
 */


/*////////////////////////////////////////////////////////////////////////// */
/*
	BUILDING AND INSTALLING PJLIB
 */



/**
 * @page pjlib_build_sys_pg Building, and Installing PJLIB
 *
 * @section build_sys_install_sec Build and Installation
 *
 * \note
 * <b>The most up-to-date information on building and installing PJLIB
 *  should be found in the website, under "Getting Started" document.
 *  More over, the new PJLIB build system is now based on autoconf,
 *  so some of the information here might not be relevant anymore 
 *  (although most still are, since the autoconf script still use
 *  the old Makefile system as the backend).</b>
 *
 * @subsection build_sys_install_win32_sec Visual Studio
 *
 * The PJLIB Visual Studio workspace supports the building of PJLIB
 * for Win32 target. Although currently only the Visual Studio 6 Workspace is
 * actively maintained, developers with later version of Visual Studio
 * can easily imports VS6 workspace into their IDE.
 *
 * To start building PJLIB projects with Visual Studio 6 or later, open
 * the \a workspace file in the corresponding \b \c build directory. You have
 * several choices on which \a dsw file to open:
 \verbatim
  $PJPROJECT/pjlib/build/pjlib.dsw
  $PJPROJECT/pjsip/build/pjsip.dsw
 ..etc
 \endverbatim
 *
 * The easiest way is to open <tt>pjsip_apps.dsw</tt> file in \b \c $PJPROJECT/pjsip-apps/build
 * directory, and build pjsua project or the samples project. 
 * However this will not build the complete projects. 
 * For example, the PJLIB test is not included in this workspace. 
 * To build the complete projects, you must
 * open and build each \a dsw file in \c build directory in each
 * subprojects. For example, to open the complete PJLIB workspace, open
 * <tt>pjlib.dsw</tt> in <tt>$PJPROJECT/pjlib/build</tt> directory.
 *
 *
 * @subsubsection config_site_create_vc_sec Create config_site.h
 *
 * The file <tt><b>$PJPROJECT/pjlib/include/pj/config_site.h</b></tt>
 * is supposed to contain configuration that is specific to your site/target.
 * This file is not part of PJLIB, so you must create it yourself. Normally
 * you just need to create a blank file.
 *
 * The reason why it's not included in PJLIB is so that you would not accidently
 * overwrite your site configuration.
 *
 * If you fail to do this, Visual C will complain with error like: 
 *
 * <b>"fatal error C1083: Cannot open include file: 'pj/config_site.h': No such file 
 * or directory"</b>.
 *
 * @subsubsection build_vc_subsubsec Build the Projects
 *
 * Just hit the build button!
 *
 *
 * @subsection build_sys_install_unix_sec Make System
 *
 * For other targets, PJLIB provides a rather comprehensive build system
 * that uses GNU \a make (and only GNU \a make will work). 
 * Currently, the build system supports building * PJLIB for these targets:
 *  - i386/Win32/mingw
 *  - i386/Linux
 *  - i386/Linux (kernel)
 *  - alpha/linux
 *  - sparc/SunOS
 *  - etc..
 *
 *
 * @subsubsection build_req_sec Requirements
 *
 * In order to use the \c make based build system, you MUST have:
 *
 *  - <b>GNU make</b>
 *\n
 *    The Makefiles heavily utilize GNU make commands which most likely
 *    are not available in other \c make system.
 *  - <b>bash</b> shell is recommended.
 *\n
 *    Specificly, there is a command <tt>"echo -n"</tt> which may not work
 *    in other shells. This command is used when generating dependencies
 *    (<tt>make dep</tt>) and it's located in 
 *    <tt>$PJPROJECT/build/rules.mak</tt>.
 *  - <b>ar</b>, <b>ranlib</b> from GNU binutils
 *\n
 *    In your system has different <tt>ar</tt> or <tt>ranlib</tt> (e.g. they
 *    may have been installed as <tt>gar</tt> and <tt>granlib</tt>), then
 *    either you create the relevant symbolic links, <b>or</b> modify
 *    <tt>$PJPROJECT/build/cc-gcc.mak</tt> and rename <tt>ar</tt> and
 *    <tt>ranlib</tt> to the appropriate names.
 *  - <b>gcc</b> to generate dependency.
 *\n
 *    Currently the build system uses <tt>"gcc -MM"</tt> to generate build
 *    dependencies. If <tt>gcc</tt> is not desired to generate dependency,
 *    then either you don't run <tt>make dep</tt>, <b>or</b> edit
 *    <tt>$PJPROJECT/build/rules.mak</tt> to calculate dependency using
 *    your prefered method. (And let me know when you do so so that I can
 *    update the file. :) )
 *
 * @subsubsection build_overview_sec Building the Project
 *
 * Generally, steps required to build the PJLIB are:
 *
 \verbatim
   $ cd /home/user/pjproject
   $ ./configure
   $ touch pjlib/include/pj/config_site.h
   $ make dep
   $ make
 \endverbatim
 *
 * The above process will build all static libraries and all applications.
 *
 * \note the <tt>configure</tt> script is not a proper autoconf script,
 * but rather a simple shell script to detect current host. This script
 * currently does not support cross-compilation.
 *
 * \note For Linux kernel target, there are additional steps required, which
 * will be explained in section \ref linux_kern_target_subsec.
 *
 * @subsubsection build_mak_sec Cross Compilation
 * 
 * For cross compilation, you will need to edit the \c build.mak file in 
 * \c $PJPROJECT root directory manually. Please see <b>README-configure</b> file
 * in the root directory for more information.
 *
 * For Linux kernel target, you are also required to declare the following
 * variables in this file:
 *	- \c KERNEL_DIR: full path of kernel source tree.
 *	- \c KERNEL_ARCH: kernel ARCH options (e.g. "ARCH=um"), or leave blank
 * 	     for default.
 *	- \c PJPROJECT_DIR: full path of PJPROJECT source tree.
 *
 * Apart from these, there are also additional steps required to build
 * Linux kernel target, which will be explained in \ref linux_kern_target_subsec.
 *
 * @subsubsection build_dir_sec Files in "build" Directory
 *
 * The <tt>*.mak</tt> files in \c $PJPROJECT/build directory are used to specify
 * the configuration for the specified compiler, target machine target 
 * operating system, and host options. These files will be executed
 * (included) by \a make during building process, depending on the values
 * specified in <b>$PJPROJECT/build.mak</b> file.
 *
 * Normally you don't need to edit these files, except when you're porting
 * PJLIB to new target.
 *
 * Below are the description of some files in this directory:
 *
 *  - <tt>rules.mak</tt>: contains generic rules always included during make.
 *  - <tt>cc-gcc.mak</tt>: rules when gcc is used for compiler.
 *  - <tt>cc-vc.mak</tt>: rules when MSVC compiler is used.
 *  - <tt>host-mingw.mak</tt>: rules for building in mingw host.
 *  - <tt>host-unix.mak</tt>: rules for building in Unix/Posix host.
 *  - <tt>host-win32.mak</tt>: rules for building in Win32 command console
 *    (only valid when VC is used).
 *  - <tt>m-i386.mak</tt>: rules when target machine is an i386 processor.
 *  - <tt>m-m68k.mak</tt>: rules when target machine is an m68k processor.
 *  - <tt>os-linux.mak</tt>: rules when target OS is Linux.
 *  - <tt>os-linux-kernel.mak</tt>: rules when PJLIB is to be build as
 *    part of Linux kernel.
 *  - <tt>os-win32.mak</tt>: rules when target OS is Win32.
 *
 *
 * @subsubsection config_site_create_sec Create config_site.h
 *
 * The file <tt><b>$PJPROJECT/pjlib/include/pj/config_site.h</b></tt>
 * is supposed to contain configuration that is specific to your site/target.
 * This file is not part of PJLIB, so you must create it yourself.
 *
 * The reason why it's not included in PJLIB is so that you would not accidently
 * overwrite your site configuration.
 *
 *
 * @subsubsection invoking_make_sec Invoking make
 *
 * Normally, \a make is invoked in \c build directory under each project.
 * For example, to build PJLIB, you would invoke \a make in
 * \c $PJPROJECT/pjlib/build directory like below:
 *
 \verbatim
   $ cd pjlib/build
   $ make
 \endverbatim
 *
 * Alternatively you may invoke <tt>make</tt> in <tt>$PJPROJECT</tt> 
 * directory, to build all projects under that directory (e.g. 
 * PJLIB, PJSIP, etc.).
 *
 *
 * @subsubsection linux_kern_target_subsec Linux Kernel Target
 *
 * \note
 * <b>BUILDING APPLICATIONS IN LINUX KERNEL MODE IS A VERY DANGEROUS BUSINESS.
 * YOU MAY CRASH THE WHOLE OF YOUR SYSTEM, CORRUPT YOUR HARDISK, ETC. PJLIB
 * KERNEL MODULES ARE STILL IN EXPERIMENTAL PHASE. DO NOT RUN IT IN PRODUCTION
 * SYSTEMS OR OTHER SYSTEMS WHERE RISK OF LOSS OF DATA IS NOT ACCEPTABLE.
 * YOU HAVE BEEN WARNED.</b>
 *
 * \note
 * <b>User Mode Linux (UML)</b> provides excellent way to experiment with Linux
 * kernel without risking the stability of the host system. See
 * http://user-mode-linux.sourceforge.net for details.
 *
 * \note
 * I only use <b>UML</b> to experiment with PJLIB kernel modules.
 * <b>I wouldn't be so foolish to use my host Linux machine to experiment
 * with this.</b> 
 *
 * \note
 * You have been warned.
 *
 * For building PJLIB for Linux kernel target, there are additional steps required.
 * In general, the additional tasks are:
 *	- Declare some more variables in <b><tt>build.mak</tt></b> file (this
 *        has been explained in \ref build_mak_sec above).
 *      - Perform these two small modifications in kernel source tree.
 *
 * There are two small modification need to be applied to the kernel tree.
 *
 * <b>1. Edit <tt>Makefile</tt> in kernel root source tree.</b>
 *
 * Add the following lines at the end of the <tt>Makefile</tt> in your 
 * <tt>$KERNEL_SRC</tt> dir:
 \verbatim
script:
       $(SCRIPT)
 \endverbatim
 *
 * \note Remember to replace spaces with <b>tab</b> in the Makefile.
 *
 * The modification above is needed to capture kernel's \c $CFLAGS and 
 * \c $CFLAGS_MODULE which will be used for PJLIB's compilation.
 *
 * <b>2. Add Additional Exports.</b>
 *
 * We need the kernel to export some more symbols for our use. So we declare
 * the additional symbols to be exported in <tt>extra-exports.c</tt> file, and add
 * a this file to be compiled into the kernel:
 *
 *	- Copy the file <tt>extra-exports.c</tt> from <tt>pjlib/src/pj</tt> 
 *	  directory to <tt>$KERNEL_SRC/kernel/</tt> directory.
 *	- Edit <tt>Makefile</tt> in that directory, and add this line
 *        somewhere after the declaration of that variable:
 \verbatim
obj-y   += extra-exports.o
 \endverbatim
 *
 * To illustrate what have been done in your kernel source tree, below
 * is screenshot of my kernel source tree _after_ the modification.
 *
 \verbatim
[root@vpc-linux linux-2.6.7]# pwd
/usr/src/linux-2.6.7
[root@vpc-linux linux-2.6.7]# 
[root@vpc-linux linux-2.6.7]# 
[root@vpc-linux linux-2.6.7]# tail Makefile 

endif   # skip-makefile

FORCE:

.PHONY: script

script:
        $(SCRIPT)

[root@vpc-linux linux-2.6.7]# 
[root@vpc-linux linux-2.6.7]# 
[root@vpc-linux linux-2.6.7]# head kernel/extra-exports.c 
#include <linux/module.h>
#include <linux/syscalls.h>

EXPORT_SYMBOL(sys_select);

EXPORT_SYMBOL(sys_epoll_create);
EXPORT_SYMBOL(sys_epoll_ctl);
EXPORT_SYMBOL(sys_epoll_wait);

EXPORT_SYMBOL(sys_socket);
[root@vpc-linux linux-2.6.7]# 
[root@vpc-linux linux-2.6.7]# 
[root@vpc-linux linux-2.6.7]# head -15 kernel/Makefile 
#
# Makefile for the linux kernel.
#

obj-y     = sched.o fork.o exec_domain.o panic.o printk.o profile.o \
            exit.o itimer.o time.o softirq.o resource.o \
            sysctl.o capability.o ptrace.o timer.o user.o \
            signal.o sys.o kmod.o workqueue.o pid.o \
            rcupdate.o intermodule.o extable.o params.o posix-timers.o \
            kthread.o

obj-y   +=  extra-exports.o

obj-$(CONFIG_FUTEX) += futex.o
obj-$(CONFIG_GENERIC_ISA_DMA) += dma.o
[root@vpc-linux linux-2.6.7]# 

 \endverbatim
 *
 * Then you must rebuild the kernel.
 * If you fail to do this, you won't be able to <b>insmod</b> pjlib.
 *
 * \note You will see a lots of warning messages during pjlib-test compilation.
 * The warning messages complain about unresolved symbols which are defined
 * in pjlib module. You can safely ignore these warnings. However, you can not
 * ignore warnings about non-pjlib unresolved symbols.
 *
 * 
 * @subsection makefile_explained_sec Makefile Explained
 *
 * The \a Makefile for each project (e.g. PJLIB, PJSIP, etc) should be
 * very similar in the contents. The Makefile is located under \c build
 * directory in each project subdir.
 *
 * @subsubsection pjlib_makefile_subsec PJLIB Makefile.
 *
 * Below is PJLIB's Makefile:
 *
 * \include build/Makefile
 *
 * @subsubsection pjlib_os_makefile_subsec PJLIB os-linux.mak.
 *
 * Below is file <tt><b>os-linux.mak</b></tt> file in 
 * <tt>$PJPROJECT/pjlib/build</tt> directory,
 * which is OS specific configuration file for Linux target that is specific 
 * for PJLIB project. For \b global OS specific configuration, please see
 * <tt>$PJPROJECT/build/os-*.mak</tt>.
 *
 * \include build/os-linux.mak
 *
 */


/*////////////////////////////////////////////////////////////////////////// */
/*
         PORTING PJLIB
 */



/**
 * @page porting_pjlib_pg Porting PJLIB
 *
 * \note
 * <b>Since version 0.5.8, PJLIB build system is now based on autoconf, so
 * most of the time we shouldn't need to apply the tweakings below to get
 * PJLIB working on a new platform. However, since the autoconf build system
 * still uses the old Makefile build system, the information below may still
 * be useful for reference.
 * </b>
 *
 * @section new_arch_sec Porting to New CPU Architecture
 *
 * Below is step-by-step guide to add support for new CPU architecture.
 * This sample is based on porting to Alpha architecture; however steps for 
 * porting to other CPU architectures should be pretty similar. 
 *
 * Also note that in this example, the operating system used is <b>Linux</b>.
 * Should you wish to add support for new operating system, then follow
 * the next section \ref porting_os_sec.
 *
 * Step-by-step guide to port to new CPU architecture:
 *  - decide the name for the new architecture. In this case, we choose
 *    <tt><b>alpha</b></tt>.
 *  - edit file <tt>$PJPROJECT/build.mak</tt>, and add new section for
 *    the new target:
 *    <pre>
 *      #
 *      # Linux alpha, gcc
 *      #
 *      export MACHINE_NAME := <b>alpha</b>
 *      export OS_NAME := linux
 *      export CC_NAME := gcc
 *      export HOST_NAME := unix
 *    </pre>
 *
 *  - create a new file <tt>$PJPROJECT/build/<b>m-alpha</b>.mak</tt>.
 *    Alternatively create a copy from other file in this directory.
 *    The contents of this file will look something like:
 *    <pre>
 *      export M_CFLAGS := $(CC_DEF)<b>PJ_M_ALPHA=1</b>
 *      export M_CXXFLAGS :=
 *      export M_LDFLAGS :=
 *      export M_SOURCES :=
 *    </pre>
 *  - create a new file <tt>$PJPROJECT/pjlib/include/pj/compat/<b>m_alpha.h</b></tt>.
 *    Alternatively create a copy from other header file in this directory.
 *    The contents of this file will look something like:
 *    <pre>
 *      #define PJ_HAS_PENTIUM          0
 *      #define PJ_IS_LITTLE_ENDIAN     1
 *      #define PJ_IS_BIG_ENDIAN        0
 *    </pre>
 *  - edit <tt>pjlib/include/pj/<b>config.h</b></tt>. Add new processor
 *    configuration in this header file, like follows:
 *    <pre>
 *      ...
 *      #elif defined (PJ_M_ALPHA) && PJ_M_ALPHA != 0
 *      #   include <pj/compat/m_alpha.h>
 *      ...
 *    </pre>
 *  - done. Build PJLIB with:
 *    <pre>
 *      $ cd $PJPROJECT/pjlib/build
 *      $ make dep
 *      $ make clean
 *      $ make
 *    </pre>
 *
 * @section porting_os_sec Porting to New Operating System Target
 *
 * This section will try to give you rough guideline on how to
 * port PJLIB to a new target. As a sample, we give the target a name tag, 
 * for example <tt><b>xos</b></tt> (for X OS). 
 *
 * @subsection new_compat_os_h_file_sec Create New Compat Header File
 *
 * You'll need to create a new header file 
 * <b><tt>include/pj/compat/os_xos.h</tt></b>. You can copy as a 
 * template other header file and edit it accordingly.
 *
 * @subsection modify_config_h_file_sec Modify config.h
 *
 * Then modify file <b><tt>include/pj/config.h</tt></b> to include
 * this file accordingly (e.g. when macro <tt><b>PJ_XOS</b></tt> is
 * defined):
 *
 \verbatim
 ...
 #elif defined(PJ_XOS)
 #  include <pj/compat/os_xos.h>
 #else
 #...
 \endverbatim
 * 
 * @subsection new_target_mak_file_sec Create New Global Make Config File
 *
 * Then you'll need to create global configuration file that
 * is specific for this OS, i.e. <tt><b>os-xos.mak</b></tt> in 
 * <tt><b>$PJPROJECT/build</b></tt> directory.
 *
 * At very minimum, the file will normally need to define
 * <tt><b>PJ_XOS=1</b></tt> in the \c CFLAGS section:
 *
 \verbatim
#
# $PJPROJECT/build/os-xos.mak:
#
export OS_CFLAGS   := $(CC_DEF)PJ_XOS=1
export OS_CXXFLAGS := 
export OS_LDFLAGS  :=
export OS_SOURCES  := 
 \endverbatim
 *
 *
 * @subsection new_target_prj_mak_file_sec Create New Project's Make Config File
 *
 * Then you'll need to create xos-specific configuration file
 * for PJLIB. This file is also named <tt><b>os-xos.mak</b></tt>,
 * but its located in <tt><b>pjlib/build</b></tt> directory.
 * This file will specify source files that are specific to
 * this OS to be included in the build process.
 *
 * Below is a sample:
 \verbatim
#
# pjlib/build/os-xos.mak:
#  XOS specific configuration for PJLIB.
#
export PJLIB_OBJS += 	os_core_xos.o \
                        os_error_unix.o \
                        os_time_ansi.o
export TEST_OBJS +=	main.o
export TARGETS	    =	pjlib pjlib-test
 \endverbatim
 *
 * @subsection new_target_src_sec Create and Edit Source Files
 *
 * You'll normally need to create at least these files:
 *  - <tt><b>os_core_xos.c</b></tt>: core OS specific
 *    functionality.
 *  - <tt><b>os_timestamp_xos.c</b></tt>: how to get timestamp
 *    in this OS.
 *
 * Depending on how things are done in your OS, you may need
 * to create these files:
 *  - <tt><b>os_error_*.c</b></tt>: how to manipulate
 *    OS error codes. Alternatively you may use existing
 *    <tt>os_error_unix.c</tt> if the OS has \c errno and
 *    \c strerror() function.
 *  - <tt><b>ioqueue_*.c</b></tt>: if the OS has specific method
 *    to perform asynchronous I/O. Alternatively you may
 *    use existing <tt>ioqueue_select.c</tt> if the OS supports
 *    \c select() function call.
 *  - <tt><b>sock_*.c</b></tt>: if the OS has specific method
 *    to perform socket communication. Alternatively you may
 *    use existing <tt>sock_bsd.c</tt> if the OS supports
 *    BSD socket API, and edit <tt>include/pj/compat/socket.h</tt>
 *    file accordingly.
 *
 * You will also need to check various files in 
 * <tt><b>include/pj/compat/*.h</b></tt>, to see if they're 
 * compatible with your OS.
 *
 * @subsection new_target_build_file_sec Build The Project
 *
 * After basic building blocks have been created for the OS, then
 * the easiest way to see which parts need to be fixed is by building
 * the project and see the error messages.
 *
 * @subsection new_target_edit_vs_new_file_sec Editing Existing Files vs Creating New File
 *
 * When you encounter compatibility errors in PJLIB during porting,
 * you have three options on how to fix the error:
 *  - edit the existing <tt>*.c</tt> file, and give it <tt>#ifdef</tt>
 *    switch for the new OS, or
 *  - edit <tt>include/pj/compat/*.h</tt> instead, or
 *  - create a totally new file.
 *
 * Basicly there is no strict rule on which approach is the best
 * to use, however the following guidelines may be used:
 *  - if the file is expected to be completely different than
 *    any existing file, then perhaps you should create a completely
 *    new file. For example, file <tt>os_core_xxx.c</tt> will 
 *    normally be different for each OS flavour.
 *  - if the difference can be localized in <tt>include/compat</tt>
 *    header file, and existing <tt>#ifdef</tt> switch is there,
 *    then preferably you should edit this <tt>include/compat</tt>
 *    header file.
 *  - if the existing <tt>*.c</tt> file has <tt>#ifdef</tt> switch,
 *    then you may add another <tt>#elif</tt> switch there. This
 *    normally is used for behaviors that are not totally
 *    different on each platform.
 *  - other than that above, use your own judgement on whether
 *    to edit the file or create new file etc.
 */

#endif	/* __PJ_DOXYGEN_H__ */

