SUBDIRS = doc src
EXTRA_DIST = GPL.txt Stanford.txt
dist_sysconf_DATA = igmpproxy.conf

ChangeLog:
	git --git-dir=$(srcdir)/.git log > $@

AUTHORS: ChangeLog
	( echo "Authors and contributors, in alphabetical order: "; echo; \
	sed -r "s/^Author: (.*)/\1/;t;d" $< | sort -u ) > $@
