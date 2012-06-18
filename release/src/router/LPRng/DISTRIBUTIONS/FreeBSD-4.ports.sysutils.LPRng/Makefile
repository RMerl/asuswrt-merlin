# New ports collection makefile for:	LPRng
# Date created:		2 Apr 1997
# Whom:			desmo@bandwidth.org
#
# $FreeBSD: ports/sysutils/LPRng/Makefile,v 1.26 2003/07/10 13:31:18 osa Exp $
#

PORTNAME=LPRng
PORTVERSION=3.8.22
CATEGORIES=	sysutils print
MASTER_SITES=	ftp://ftp.lprng.com/pub/%SUBDIR%/ \
		ftp://ftp.cise.ufl.edu/pub/mirrors/%SUBDIR%/ \
		ftp://ftp.cs.umn.edu/pub/%SUBDIR%/ \
		ftp://ftp.informatik.uni-hamburg.de/pub/os/unix/utils/%SUBDIR%/ \
		ftp://ftp.uni-paderborn.de/pub/unix/printer/%SUBDIR%/
MASTER_SITE_SUBDIR=	LPRng/LPRng
EXTRACT_SUFX=	.tgz

MAINTAINER=	papowell@astart.com
COMMENT=	An Enhanced Printer Spooler

GNU_CONFIGURE=	yes
#USE_LIBTOOL=	yes
INSTALLS_SHLIB=	yes

CONFIGURE_ARGS=	\
		--with-filterdir=${PREFIX}/libexec/filters \
		--with-filter_path="${PREFIX}/bin:/bin:/usr/bin:${PREFIX}/sbin:/sbin:/usr/sbin" \
		LDFLAGS="-L${LOCALBASE}/lib" \
		CPPFLAGS="-I${LOCALBASE}/include" \
		--with-ld_library_path="${PREFIX}/lib:/lib:/usr/lib:/${LOCALBASE}/lib"
.if defined(PREFIX)
  CONFIGURE_ARGS+=  --prefix="${PREFIX}"
.endif
.if defined(SYSCONFDIR)
  CONFIGURE_ARGS+=  --sysconfdir="${SYSCONFDIR}"
.endif

MAN1=cancel.1 lp.1 lpbanner.1 lpf.1 lpq.1 lpr.1 lprm.1 lprng_certs.1 lprng_index_certs.1 lpstat.1 monitor.1 pclbanner.1 psbanner.1
MAN5=lpd.conf.5 lpd.perms.5 printcap.5
MAN8=checkpc.8 lpc.8 lpd.8

pre-everything::
	@${ECHO_MSG} "If you want to replace the default printing system with LPRng, use:"
	@${ECHO_MSG} "  make PREFIX=/usr SYSCONFDIR=/etc clean all install"
	@${ECHO_MSG} " OR"
	@${ECHO_MSG} "  make CONFIGURE_SCRIPT=STANDARD_configuration clean all install"
	@if [ "${PREFIX}" = "/usr" -a ! -d /usr/man ] ; then \
		${ECHO_MSG} "The man pages will be installed in /usr/man." ; \
		${ECHO_MSG} "You should make a symbolic link /usr/share/man from /usr/man"; \
		${ECHO_MSG} "   ln -s /usr/share/man /usr/man"; \
		${ECHO_MSG} "If you do not, you will retain the old FreeBSD man pages."; \
		${ECHO_MSG} "See the hier(7) man page for details of the FreeBSD file system"; \
		${ECHO_MSG} "layout.  Configure is not equipped to determine the location of"; \
		${ECHO_MSG} 'man pages and defaults to $${PREFIX}/man, which is incorrect for FreeBSD.'; \
	fi

post-install:
.if !defined(NOPORTSDOCS)
	@${INSTALL} -d -o ${SHAREOWN} -g ${SHAREGRP} -m 0555 ${DOCSDIR}
	@${INSTALL} -d -o ${SHAREOWN} -g ${SHAREGRP} -m 0555 ${DOCSDIR}/Reference
	@${INSTALL} -d -o ${SHAREOWN} -g ${SHAREGRP} -m 0555 ${DOCSDIR}/PrintingCookbook
.for ext in html pdf ppt ps txt gif jpg png
	if test -n "VERBOSE_INSTALL"; then set -x; fi; \
	src=${WRKSRC}/DOCS; \
	for i in `ls $${src} | ${GREP} "\.${ext}$$"`; \
		do ${INSTALL_DATA} $${src}/$$i ${DOCSDIR}/Reference; done; \
	src=${WRKSRC}/PrintingCookbook/HTML; \
	for i in `ls $${src} | ${GREP} "\.${ext}$$"`; \
		do ${INSTALL_DATA} $${src}/$$i ${DOCSDIR}/PrintingCookbook; done;
.endfor
	if test -n "VERBOSE_INSTALL"; then set -x; fi; \
		${INSTALL_DATA} ${WRKSRC}/PrintingCookbook/PDF/*.pdf ${DOCSDIR}/PrintingCookbook; done
.endif
	@${SED} -e "s!DOCSDIR!${DOCSDIR}/!" ${PKGMESSAGE}

.include <bsd.port.mk>
