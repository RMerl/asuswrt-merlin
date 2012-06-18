## -- Config --
DEV := lo
PREFIX := sudo
## -- End Config --

TESTS := $(patsubst tests/%,%,$(wildcard tests/*.t))
IPVERS := $(filter-out iproute2/Makefile,$(wildcard iproute2/*))
KENV := $(shell cat /proc/config.gz | gunzip | grep ^CONFIG)

.PHONY: compile listtests alltests configure $(TESTS)

configure:
	echo "Entering iproute2" && cd iproute2 && $(MAKE) configure && cd ..;

compile: configure
	echo "Entering iproute2" && cd iproute2 && $(MAKE) && cd ..;

listtests:
	@for t in $(TESTS); do \
		echo "$$t"; \
	done

alltests: $(TESTS)

clean:
	@rm -rf results/*

distclean: clean
	echo "Entering iproute2" && cd iproute2 && $(MAKE) distclean && cd ..;

$(TESTS):
	@for i in $(IPVERS); do \
		o=`echo $$i | sed -e 's/iproute2\///'`; \
		echo -n "Running $@ [$$o/`uname -r`]: "; \
		TC="$$i/tc/tc" IP="$$i/ip/ip" DEV="$(DEV)" IPVER="$@" SNAME="$$i" \
		ERRF="results/$@.$$o.err" $(KENV) $(PREFIX) tests/$@ > results/$@.$$o.out; \
		if [ "$$?" = "127" ]; then \
			echo "SKIPPED"; \
		elif [ -e "results/$@.$$o.err" ]; then \
			echo "FAILED"; \
		else \
			echo "PASS"; \
		fi; \
		dmesg > results/$@.$$o.dmesg; \
	done
