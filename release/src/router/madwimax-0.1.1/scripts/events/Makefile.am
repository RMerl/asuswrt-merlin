if WITH_SCRIPT

eventsdir = $(sysconfdir)
events_SCRIPTS = event.sh

event.sh: event.sh.$(script)
	cp event.sh.$(script) event.sh

MOSTLYCLEANFILES = event.sh
DISTCLEANFILES = event.sh.$(script)

endif

EXTRA_DIST = event.sh.generic.in event.sh.wl500g.in
