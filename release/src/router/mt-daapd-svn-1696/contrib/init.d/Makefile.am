EXTRA_DIST=mt-daapd-fedora.templ mt-daapd-suse.templ mtdaapd-bsd.templ

BUILT_SOURCES = mt-daapd-fedora mt-daapd-suse mtdaapd-bsd

mt-daapd-fedora: mt-daapd-fedora.templ ../../config.h
	sed -e s,[@]prefix[@],$(prefix),g mt-daapd-fedora.templ >$@; \
	chmod +x $@

mt-daapd-suse: mt-daapd-suse.templ ../../config.h
	sed -e s,[@]prefix[@],$(prefix),g mt-daapd-suse.templ >$@; \
	chmod +x $@

mtdaapd-bsd: mtdaapd-bsd.templ ../../config.h
	sed -e s,[@]prefix[@],$(prefix),g mtdaapd-bsd.templ >$@; \
	chmod +x $@




