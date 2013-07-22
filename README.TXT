
The GPL source is created for ASUS wireless router related products. Please visit the ASUS support site (http://support.asus.com) to get the latest GPL tarball.It has a lot in common with many wireless router open source projects, including Oleg/Tomato/DD-WRT/OpenWRT. Thanks the developers of those projects for making the source code available.

Set Up Environment (Tested in Fedora 8/9 and Ubuntu 10.04/12.04 LTS amd64)

    1. prepare environment
	
	a. Ubuntu
		Install these packages (I used synaptic: "sudo synaptic")

		bison
		flex
		g++
		g++-4.4
		g++-multilib
		gawk
		gcc-multilib
		gconf (or gconf-editor)
		gitk
		lib32z1-dev
		libncurses5
		libncurses5-dev
		libstdc++6-4.4-dev
		libtool
		m4
		pkg-config
		zlib1g-dev
		autoconf
		autopoint
		libtool
		shtool
		autogen
		gperf
		lib32z1-dev
		libssl-dev
		libxml2-dev
		libelf1:i386
		mtd-utils

	b. Fedora
		sudo yum groupinstall "Development Tools"
 		sudo yum install gettext-devel libxml2-devel mtd-utils-ubi ncurses-devel zlib-devel

		If you are using Fedora 19, you need to install below packages.
		sudo yum groupinstall "C Development Tools and Libraries"

 		If you are using Fedora x86_64, you may need to install 32-bit packages listed below.
 			glibc.i686
 			libstdc++.i686
 			zlib.i686

   2. prepare source to, ex, $HOME/asuswrt

	cd $HOME
	tar xvfz [tar file]
 
   3. setup development system

	ln -s $HOME/asuswrt/tools/brcm /opt/brcm
	export PATH=$PATH:/opt/brcm/hndtools-mipsel-linux/bin:/opt/brcm/hndtools-mipsel-uclibc/bin:/opt/asuswrt/tools/brcm/hndtools-arm-linux/bin

	Note: Broadcom/Ralink platform use the same toolchain for user space program, so please set PATH to the same directory as above
   
  4. build firmware.
	a. rt-n16
		cd release/src-rt
		make rt-n16

	b. rt-n56u
		cd release/src-ra
		make rt-n56u

	c. rt-n65u
		cd release/src-ra-3.0
		make rt-n65u

 	d. rt-n14u
 		cd release/src-ra-mt7620
 		make rt-n14u
 
 	e. rt-ac56u (/ rt-ac68u / rt-n18uhp)
 		cd release/src-rt-6.x.4708
 		make rt-ac56u
 		( make rt-ac68u  )
 		( make rt-n18uhp )
