# $Id: Makefile.am,v 1.1 2007-08-16 07:22:17 yy Exp $
#
# "Makefile.am" for "libunp/upnp/doc" 
#
# (C) Copyright 2005 Rémi Turboult <r3mi@users.sourceforge.net>
#
##########################################################################
#
# Copyright (c) 2000-2003 Intel Corporation 
# All rights reserved. 
#
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the following conditions are met: 
#
# * Redistributions of source code must retain the above copyright notice, 
# this list of conditions and the following disclaimer. 
# * Redistributions in binary form must reproduce the above copyright notice, 
# this list of conditions and the following disclaimer in the documentation 
# and/or other materials provided with the distribution. 
# * Neither name of Intel Corporation nor the names of its contributors 
# may be used to endorse or promote products derived from this software 
# without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
# ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL INTEL OR 
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR 
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY 
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS 
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
##########################################################################

EXTRA_DIST		= UPnP_Programming_Guide.pdf \
			  callback.dxx docxx.sty intro.dxx \
			  license.dxx upnpsdk.dxx

html-local:
	@if [ -d html ]; then rm -rf html; fi
	@doc++ -nd -S -w -j -d html $(srcdir)/upnpsdk.dxx

pdf-local:
	@doc++ -nd -S -w -j -t --package a4wide -o upnpsdk.tex \
		$(srcdir)/upnpsdk.dxx
	@-pdflatex "\scrollmode\input upnpsdk.tex" > latex.log
	@-pdflatex "\scrollmode\input upnpsdk.tex" >> latex.log
	@-pdflatex "\scrollmode\input upnpsdk.tex" >> latex.log

clean-local:
	-rm -rf html
	-rm -f upnpsdk.tex upnpsdk.dvi upnpsdk.ps upnpsdk.log upnpsdk.aux 
	-rm -f latex.log



