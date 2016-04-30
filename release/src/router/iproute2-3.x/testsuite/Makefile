## -- Config --
DEV := lo
PREFIX := sudo -E
RESULTS_DIR := results
## -- End Config --

rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))

TESTS := $(patsubst tests/%,%,$(call rwildcard,tests/,*.t))
TESTS_DIR := $(dir $(TESTS))

IPVERS := $(filter-out iproute2/Makefile,$(wildcard iproute2/*))

ifneq (,$(wildcard /proc/config.gz))
	KENV := $(shell cat /proc/config.gz | gunzip | grep ^CONFIG)
endif

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
	@echo "Removing $(RESULTS_DIR) dir ..."
	@rm -rf $(RESULTS_DIR)

distclean: clean
	echo "Entering iproute2" && cd iproute2 && $(MAKE) distclean && cd ..;

$(TESTS): clean
	@mkdir -p $(RESULTS_DIR)
	
	@for d in $(TESTS_DIR); do \
	    mkdir -p $(RESULTS_DIR)/$$d; \
	done
	
	@for i in $(IPVERS); do \
		o=`echo $$i | sed -e 's/iproute2\///'`; \
		echo -n "Running $@ [$$o/`uname -r`]: "; \
		TC="$$i/tc/tc" IP="$$i/ip/ip" DEV="$(DEV)" IPVER="$@" SNAME="$$i" \
		ERRF="$(RESULTS_DIR)/$@.$$o.err" $(KENV) $(PREFIX) tests/$@ > $(RESULTS_DIR)/$@.$$o.out; \
		if [ "$$?" = "127" ]; then \
			echo "SKIPPED"; \
		elif [ -e "$(RESULTS_DIR)/$@.$$o.err" ]; then \
			echo "FAILED"; \
		else \
			echo "PASS"; \
		fi; \
		dmesg > $(RESULTS_DIR)/$@.$$o.dmesg; \
	done
