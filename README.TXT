
The GPL source is created for ASUS wireless router related products. Please visit the ASUS support site (http://support.asus.com) to get the latest GPL tarball.It has a lot in common with many wireless router open source projects, including Oleg/Tomato/DD-WRT/OpenWRT. Thanks the developers of those projects for making the source code available.

Set Up Environment (Tested in Fedora 8/9 and Ubuntu 10.04/12.04 LTS amd64)

    1. prepare environment
	
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
	gperf
	lib32z1-dev
	libssl-dev

   2. prepare source to, ex, $HOME/asuswrt
	cd $HOME
	tar xvfz [tar file]
 
   3. setup development system
	ln -s $HOME/asuswrt/tools/brcm /opt/brcm
        export PATH=$PATH:/opt/brcm/hndtools-mipsel-linux/bin:/opt/brcm/hndtools-mipsel-uclibc/bin
	mkdir -p /media/ASUSWRT/
	ln -s ~/asuswrt /media/ASUSWRT/asuswrt

	Note: Broadcom/Ralink platform use the same toolchain for user space program, so please set PATH to the same directory as above
   
   4. build firmware, ex, rt-n16
	cd ~/asuswrt/release/src-rt
	make rt-n16
