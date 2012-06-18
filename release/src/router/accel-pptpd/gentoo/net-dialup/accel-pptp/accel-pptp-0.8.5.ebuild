# Copyright 1999-2006 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: /var/cvsroot/gentoo-x86/net-dialup/pptpd/pptpd-1.3.1.ebuild,v 1.1 2006/03/26 09:13:06 mrness Exp $

inherit linux-mod eutils autotools

DESCRIPTION="Point-to-Point Tunnelling Protocol Client/Server for Linux"
SRC_URI="mirror://sourceforge/accel-pptp/${P}.tar.bz2"
HOMEPAGE="http://accel-pptp.sourceforge.net/"

SLOT="0"
LICENSE="GPL"
KEYWORDS="~amd64 ~x86"
IUSE="tcpd server"

DEPEND="server? ( !net-dialup/pptpd ) 
        >=net-dialup/ppp-2.4.2
        >=virtual/linux-sources-2.6.15
	tcpd? ( sys-apps/tcp-wrappers )"
RDEPEND="virtual/modutils"

MODULE_NAMES="pptp(misc:${S}/kernel/driver)"
BUILD_TARGETS="default"
BUILD_PARAMS="KDIR=${KERNEL_DIR}"
CONFIG_CHECK="PPP PPPOE"
MODULESD_PPTP_ALIASES=("net-pf-24 pptp")

src_unpack() {
	unpack ${A}

	local PPPD_VER=`best_version net-dialup/ppp`
	PPPD_VER=${PPPD_VER#*/*-} #reduce it to ${PV}-${PR}
	PPPD_VER=${PPPD_VER%%[_-]*} # main version without beta/pre/patch/revision
	#Match pptpd-logwtmp.so's version with pppd's version (#89895)
	#sed -i -e "s:\\(#define[ \\t]*VERSION[ \\t]*\\)\".*\":\\1\"${PPPD_VER}\":" "${S}/pptpd-1.3.3/plugins/patchlevel.h"
	#sed -i -e "s:\\(#define[ \\t]*PPP_VERSION[ \\t]*\\)\".*\":\\1\"${PPPD_VER}\":" "${S}/pppd_plugin/src/pppd/patchlevel.h"
	
	convert_to_m ${S}/kernel/driver/Makefile

	cd ${S}/pppd_plugin
	eautoreconf

	if use server; then
	    cd ${S}/pptpd-1.3.3
	    eautoreconf
	fi
}

src_compile() {
	
	if use server; then
	    cd ${S}/pptpd-1.3.3
	    local myconf
	    use tcpd && myconf="--with-libwrap"
	    econf --with-bcrelay \
		${myconf} || die "configure failed"
	    emake COPTS="${CFLAGS}" || die "make failed"
	fi
	
	cd ${S}/pppd_plugin
	local myconf
	econf ${myconf} || die "configure failed"
	emake COPTS="${CFLAGS}" || die "make failed"
	
	cd ${S}/kernel/driver
	linux-mod_src_compile || die "failed to build driver"
}

src_install () {
	if use server; then
	    cd ${S}/pptpd-1.3.3
	    einstall || die "make install failed"

	    insinto /etc
	    doins samples/pptpd.conf

	    insinto /etc/ppp
	    doins samples/options.pptpd

	    exeinto /etc/init.d
	    newexe "${FILESDIR}/pptpd-init" pptpd

	    insinto /etc/conf.d
	    newins "${FILESDIR}/pptpd-confd" pptpd
	fi
	
	if use client; then
	    cd ${S}/example
	    insinto /etc/ppp
	    doins ppp/options.pptp
	    insinto /etc/ppp/peers
	    doins ppp/peers/pptp_test
	fi

	cd ${S}/pppd_plugin/src/.libs
	local PPPD_VER=`best_version net-dialup/ppp`
	PPPD_VER=${PPPD_VER#*/*-} #reduce it to ${PV}-${PR}
	PPPD_VER=${PPPD_VER%%[_-]*} # main version without beta/pre/patch/revision
	insinto /usr/lib/pppd/${PPPD_VER}
	newins pptp.so.0.0.0 pptp.so

	cd ${S}/kernel/driver
	linux-mod_src_install
	
	cd ${S}
	dodoc README
	cp -R example "${D}/usr/share/doc/${P}/exmaple"
}

pkg_postinst () {
    modules-update
}
