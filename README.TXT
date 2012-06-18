
The GPL source is created for ASUS wireless router related products. Please visit the ASUS support site (http://support.asus.com) to get the latest GPL tarball.It has a lot in common with many wireless router open source projects, including Oleg/Tomato/DD-WRT/OpenWRT. Thanks the developers of those projects for making the source code available.

Set Up Environment(Tested in Fedora 8/9 and Ubuntu)

    1. prepare environment
	
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
	git
	gitk
	zlib1g-dev
   
   2. prepare source to, ex, $HOME/asuswrt
	cd $HOME
	tar xvfz [tar file]
 
   3. setup development system
	ln -s $HOME/asuswrt/tools/brcm /opt/brcm
        export PATH=$PATH:/opt/brcm/hndtools-mipsel-linux/bin:/opt/brcm/hndtools-mipsel-uclibc/bin
        ln -s $HOME/asuswrt/tools/buildroot-gcc342 /opt/buildroot-gcc342

	Note: Broadcom/Ralink platform use the same toolchain for user space program, so please set PATH to the same directory as above
   
   4. build firmware, ex, rt-n16
	cd release/src-rt/
	make rt-n16

	
