# This file is part of avahi.
#
# avahi is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 2 of the
# License, or (at your option) any later version.
#
# avahi is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
# License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with avahi; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
# USA.

EXTRA_DIST=build-db.in service-types

pkglibdatadir=$(libdir)/avahi

pkgdata_DATA=service-types
pkglibdata_DATA=

if HAVE_PYTHON
if HAVE_GDBM

noinst_SCRIPTS=build-db
pkglibdata_DATA+=service-types.db

build-db: build-db.in
	$(AM_V_GEN)sed -e 's,@PYTHON\@,$(PYTHON),g' \
	    -e 's,@DBM\@,gdbm,g' $< > $@ && \
	chmod +x $@

service-types.db: service-types build-db
	$(AM_V_GEN)$(PYTHON) build-db $< $@.coming && \
	mv $@.coming $@

CLEANFILES = service-types.db build-db

endif
if HAVE_DBM

noinst_SCRIPTS=build-db
pkglibdata_DATA+=service-types.db.pag service-types.db.dir

build-db: build-db.in
	$(AM_V_GEN)sed -e 's,@PYTHON\@,$(PYTHON),g' \
	    -e 's,@DBM\@,dbm,g' $< > $@ && \
	chmod +x $@

service-types.db.pag: service-types.db
	$(AM_V_GEN)mv service-types.db.coming.pag service-types.db.pag
service-types.db.dir: service-types.db
	$(AM_V_GEN)mv service-types.db.coming.dir service-types.db.dir
service-types.db: service-types build-db
	$(AM_V_GEN)$(PYTHON) build-db $< $@.coming && \
	if test -f "$@.coming"; then mv $@.coming $@; fi

CLEANFILES = service-types.db* build-db

endif
endif
