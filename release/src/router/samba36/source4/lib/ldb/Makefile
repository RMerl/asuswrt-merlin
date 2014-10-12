# simple makefile wrapper to run waf

WAF=WAF_MAKE=1 PATH=buildtools/bin:../../../buildtools/bin:$$PATH waf

all:
	$(WAF) build

install:
	$(WAF) install

uninstall:
	$(WAF) uninstall

test:
	$(WAF) test $(TEST_OPTIONS)

dist:
	touch .tmplock
	WAFLOCK=.tmplock $(WAF) dist

distcheck:
	touch .tmplock
	WAFLOCK=.tmplock $(WAF) distcheck

clean:
	$(WAF) clean

distclean:
	$(WAF) distclean

reconfigure: configure
	$(WAF) reconfigure

show_waf_options:
	$(WAF) --help

# some compatibility make targets
everything: all

testsuite: all

check: test

# this should do an install as well, once install is finished
installcheck: test

etags:
	$(WAF) etags

ctags:
	$(WAF) ctags
