Build instructions
==================

Please refer to the Wiki for detailed instructions on how to 
build the firmware from source:

https://github.com/RMerl/asuswrt-merlin/wiki#development

A Linux-based environment is required.  The firmware has been 
successfully built under Mint-Linux and Ubuntu.  As of 
July 2016, the official Asuswrt-Merlin build environment 
is based on Ubuntu 16.04 LTS.


Installation:
-------------

1) Make sure your build environment has all the necessary 
   software packages installed.  Under Ubuntu 16.04 LTS:

      sudo apt-get install git autoconf automake bash bison bzip2 diffutils file flex \
      m4 g++ gawk groff-base libncurses-dev libtool libslang2 make patch perl \
      pkg-config shtool subversion tar texinfo zlib1g zlib1g-dev git-core gettext \
      libexpat1-dev libssl-dev cvs gperf unzip python libxml-parser-perl gcc-multilib \
      gconf-editor libxml2-dev g++-4.7 g++-multilib gitk libncurses5 mtd-utils \
      libncurses5-dev libstdc++6-4.7-dev libvorbis-dev g++-4.7-multilib git autopoint \
      autogen sed build-essential intltool libelf1:i386 libglib2.0-dev

   If your build environment is 64-bit, then also install the following packages:

      sudo apt-get install lib32z1-dev lib32stdc++6


2) Setup build environment and toolchain.  If the source code is located under
   ~/asuswrt-merlin/ :

      sudo ln -s ~/asuswrt-merlin/tools/brcm /opt/brcm
      sudo ln -s ~/asuswrt-merlin/release/src-rt-6.x.4708/toolchains/hndtools-arm-linux-2.6.36-uclibc-4.5.3 /opt/brcm-arm

      echo "PATH=$PATH:/opt/brcm/hndtools-mipsel-linux/bin:/opt/brcm/hndtools-mipsel-uclibc/bin:/opt/brcm-arm/bin" >> ~/.profile


3) Either use the supplied tools/build-all script (read it
   to see how to configure and use it), or manually launch
   the build process from the appropriate SDK location.

   MIPS (RT-N66U, RT-AC66U):
      cd release/src-rt-6.x/
      make rt-n66u

   ARM SDK6 (RT-AC56U, RT-AC68U, RT-AC87U):
      cd release/src-rt-6.x.4708/
      make rt-ac56u

   ARM SDK7 (RT-AC3200)
      cd release/src-rt-7.x.main/src/
      make rt-ac3200u

   ARM SDK7.14 (RT-AC88U, RT-AC3100, RT-AC5300)
      cd release/src-rt-7.14.114.x/src/
      make rt-ac88u


   The resulting FW image will be in the image/ 
   sub-directory.

