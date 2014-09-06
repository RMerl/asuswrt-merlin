1. INTRODUCTION 

   Last revision 05/30/2003	

   This document describes the process to build net-snmp-5.0.8 stack for embedded linux 
   platforms based on the following Matsushita(Panasonic) processors family.

   AM33,AM34
   MN10300,MN103E0HRA
  
   The same procedure can be followed to build the net-snmp stack for other Matsushita 
   family of processors also.

2. ENVIRONMENT
   
   Host Machine      : Linux 7.1 or later ( with nfs server or samba server installed ).
   Target Machine    : Am33 Based Embedded platform.
   Cross-compiler    : GNU compiler version 3.1 for AM33/AM34/MN10300/MN103E010HRA
                       am33_2.0-linux-gnu-gcc
   Host-Target Setup : Samba mount or NFS mount
      

2. CONFIGURATION
   
   The following configuration flags can be used to create Makefile.You can reaplce
   some of the configuration flags according to your platform and compiler.

   Perl support was NOT compiled in due to unavailability of perl support for 
   AM3X platform at this time.

   The parameters passed to configure are as follows...
   ( you can down load the script configure.am33 script )

   --with-cc=am33_2.0-linux-gnu-gcc 
   --host=i686-pc-linux-gnu 
   --target=am33-linux ( Can be removed, if it stops building process )
   --disable-dlopen 
   --disable-dlclose 
   --disable-dlerror 
   --with-endianness=little 
   --with-openssl=no 
   --with-cflags="-g -mam33 -O2 -static" 
   --oldincludedir=./usr/local 
   --prefix=./usr/local 
   --exec-prefix=./usr/local 
   --with-persistent-directory=./usr/local

   These parameters passed are depending on the capabilities available for the
   AM33/AM34 development environment at the time of build. These parameter can be 
   changed depending on the avialable capabilities and desired preferences.
  
   You can use the below shell script directly to create Makefiles and other files.
   This script also insttals all binaries ,libraries in usr directory in the directory 
   in which this scrip executed.

# configure.am33 
#--------------------------------------------------------------------------
./configure --with-cc=am33_2.0-linux-gnu-gcc --host=i686-pc-linux-gnu \
--disable-dlopen --target=am33-linux --disable-dlclose --disable-dlerror \
--with-endianness=little --with-openssl=no --with-cflags="-g -mam33 -O2 -static" \
--oldincludedir=./usr/local --prefix=./usr/local --exec-prefix=./usr/local \
--with-persistent-directory=./usr/local

make 
make install
#--------------------------------------------------------------------------

2. INSTALLATION

   Find a partition with 60 Mb available space which will be mounted on to target machine.
   Copy or ftp the binary to this location ( copy entire usr directory tree ). 
   Copy net-snmp configuration files from host machine (.snmp directory) on to target / directory.
   snmp configuration files can be created on host machine by running sbmpconf command. Make sure 
   that host is using snmpconf from net-snmp-5.0.8 version.
        
   -:ON AM3X target Shell :-
   Mount the above directory on AM3X platform either using NFS or sambs clients on target machine.
   
   If you are running a previous version, stop the daemon

   ps -ef | grep snmp

   will return something like:

   root 17736 1 - Jan 26 ? 0:00 /usr/local/sbin/snmpd

   the PID is 17736, so you need to type

   kill {PID}

   in our example this would be

   kill 17736.

   cd /usr/local/sbin
   ./snmpd
   
2.  TESTING

   You will need to know your SNMP community.  For this example, we will use "public".

   snmpwalk -v 2c -m ALL -c public -t 100 localhost .1.3 > snmpwalk.txt
   more snmpwalk.txt

   This should return a considerable amount of output.
  
3. ISSUES
   
   You may not see correct target name in the build summary. Just ignore it.


   Please refer net-snmp documentation for more information...


Srinivasa Rao Gurusu
Engineer
Panasonic Semiconductor Development Center ( PSDC )
gurusus@research.panasonic.com
