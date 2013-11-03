SUBDIRS = manpages manual

DISTCLEANFILES = html.xsl man.xsl

release-notes: www/ReleaseNotes
	cd www && ./create-relnotes.sh

upload-release-notes: release-notes
	scp www/ReleaseNotes.html $$USER,netatalk@web.sourceforge.net:/home/project-web/netatalk/htdocs/3.0/ReleaseNotes$(VERSION).html
