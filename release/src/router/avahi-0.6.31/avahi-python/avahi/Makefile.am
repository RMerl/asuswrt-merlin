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

EXTRA_DIST = __init__.py ServiceTypeDatabase.py.in

pkglibdatadir=$(libdir)/avahi

if HAVE_PYTHON

avahidir = $(pythondir)/avahi

if HAVE_GDBM
nodist_avahi_SCRIPTS = ServiceTypeDatabase.py

ServiceTypeDatabase.py: ServiceTypeDatabase.py.in
	$(AM_V_GEN)sed -e 's,@PYTHON\@,$(PYTHON),g' \
		-e 's,@DBM\@,gdbm,g' \
		-e 's,@FIRST_KEY\@,key = self.db.firstkey(),g' \
		-e 's,@CHECK_KEY\@,while key is not None:,g' \
		-e 's,@NEXT_KEY\@,key = self.db.nextkey(key),g' \
		-e 's,@pkglibdatadir\@,$(pkglibdatadir),g' $< > $@ && \
	chmod +x $@
endif

if HAVE_DBM
nodist_avahi_SCRIPTS = ServiceTypeDatabase.py

ServiceTypeDatabase.py: ServiceTypeDatabase.py.in
	$(AM_V_GEN)sed -e 's,@PYTHON\@,$(PYTHON),g' \
		-e 's,@DBM\@,dbm,g' \
		-e 's,@FIRST_KEY\@,keys = self.db.keys(),g' \
		-e 's,@CHECK_KEY\@,for key in keys:,g' \
		-e 's,@NEXT_KEY\@,,g' \
		-e 's,@pkglibdatadir\@,$(pkglibdatadir),g' $< > $@ && \
	chmod +x $@
endif

avahi_PYTHON = $(avahi_SCRIPTS)

if HAVE_PYTHON_DBUS

avahi_PYTHON += __init__.py

endif
endif

CLEANFILES=*.pyc *.pyo ServiceTypeDatabase.py
