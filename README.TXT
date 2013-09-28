
The GPL source is created for ASUS wireless router related products. Please visit the ASUS support site (http://support.asus.com) to get the latest GPL tarball.It has a lot in common with many wireless router open source projects, including Oleg/Tomato/DD-WRT/OpenWRT. Thanks the developers of those projects for making the source code available.

Set Up Environment (Tested in Fedora 8/9 and Ubuntu 10.04/12.04 LTS amd64)

    1. prepare environment
	
	a. Ubuntu
		Install these packages (I used synaptic: "sudo synaptic")

		libncurses5
		libncurses5-dev
		m4
		bison
		gawk
		flex
		libstdc++6-4.4-dev
		g++-4.4-multilib
		g++
		git
		gitk
		zlib1g-dev
		autoconf
		autopoint
		libtool
		shtool
		autogen
		mtd-utils
		gcc-multilib
		gconf (or gconf-editor)
		lib32z1-dev
		pkg-config
		gperf
		libssl-dev
		libxml2-dev
		libelf1:i386

	b. Fedora
		sudo yum groupinstall "Development Tools"
 		sudo yum install gettext-devel libxml2-devel mtd-utils-ubi ncurses-devel zlib-devel gperf

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
	ln -s $HOME/asuswrt/release/src-rt-6.x.4708/toolchains/hndtools-arm-linux-2.6.36-uclibc-4.5.3 /opt/brcm-arm
        export PATH=$PATH:/opt/brcm/hndtools-mipsel-linux/bin:/opt/brcm/hndtools-mipsel-uclibc/bin:/opt/brcm-arm/bin

	Note: Broadcom (except 4708 series) platform use the same toolchain for user space program, so please set PATH to the same directory as above
   
  4. build firmware.
	a. rt-n16
		cd release/src-rt
		make rt-n16

	b. rt-ac66u
		cd release/src-rt-6.x
		make rt-ac66u

 	c. rt-ac56u (/ rt-ac68u / rt-n18uhp)
 		cd release/src-rt-6.x.4708
 		make rt-ac56u
 		( make rt-ac68u  )
 		( make rt-n18uhp )
