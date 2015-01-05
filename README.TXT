
The GPL source is created for ASUS wireless router related products. Please visit the ASUS support site (http://support.asus.com) to get the latest GPL tarball.It has a lot in common with many wireless router open source projects, including Oleg/Tomato/DD-WRT/OpenWRT. Thanks the developers of those projects for making the source code available.

Set Up Environment(Tested in Fedora 8/9 and Ubuntu)

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
		g++-4.4
		g++
		git (or git-core)
		gitk
		zlib1g-dev
		autoconf
		autopoint (or gettext)
		libtool
		shtool
		autogen
		mtd-utils
		intltool
		sharutils
		docbook-xsl-*
		libstdc++5
		texinfo
		dos2unix (or tofrodos)
		xsltproc

	b. Fedora
		sudo yum groupinstall "Development Tools"
		sudo yum install gettext-devel libxml2-devel mtd-utils-ubi ncurses-devel zlib-devel intltool sharutils docbook-dtds docbook-style-xsl

		If you are using Fedora 19 or above, you need to install below packages.
		sudo yum groupinstall "C Development Tools and Libraries"

		If you are using Fedora x86_64, you may need to install 32-bit packages listed below.
			glibc.i686
			libstdc++.i686
			zlib.i686
   
   2. prepare source to, ex, $HOME/asuswrt

	cd $HOME
	tar xvfz [tar file]
 
   3. setup development system

	Broadcom SoC models
	===================

		To install the tools:
	    	    - copy the tools/brcm/ directory to /opt
		    - add /opt/brcm/hndtools-mipsel-linux/bin to your path
		    - add /opt/brcm/hndtools-mipsel-uclibc/bin to your path

	Mediatek/Ralink SoC models
	==========================

		To install the tools:
	    	    - copy the tools/brcm/ directory to /opt
		    - add /opt/brcm/hndtools-mipsel-linux/bin to your path
		    - add /opt/brcm/hndtools-mipsel-uclibc/bin to your path
	    	    - extract tools/buildroot-gcc342.tar.bz2 to /opt
		    - add /opt/buildroot-gcc342/bin to your path

		For MT7621 Uboot:
	    	    - extract mips-2012.03.tar.bz2 directory to /opt
		    - add /opt/mips-2012.03/bin to your uboot path

	Qualcomm SoC models
	===================

		To install the tools:
	    	    - extract openwrt-gcc463.mips.tar.bz2 directory to /opt
		    - add /opt/openwrt-gcc463.mips/bin to your path
		    - If you want to build small utilities out of asuswrt box,
		      add STAGING_DIR environment variable as below:

		      export STAGING_DIR=/opt/openwrt-gcc463.mips
	

	Note: Broadcom/Ralink(except 4708 series) platform use the same toolchain for user space program, so please set PATH to the same directory as above
   
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
	
	d. rt-n14u (/ rt-ac52u / rt-ac51u / rt-n11p / rt-n54u)
		cd release/src-ra-mt7620
		make rt-n14u
		( make rt-ac52u  )
		( make rt-ac51u  )
		( make rt-n11p   )
		( make rt-n54u   )

	e. rt-ac56u (/ rt-ac68u / rt-n18uhp)
		cd release/src-rt-6.x.4708
		make rt-ac56u
		( make rt-ac68u  )
		( make rt-n18uhp )

	f. rt-ac55u
		cd release/src-qca
		make rt-ac55u
