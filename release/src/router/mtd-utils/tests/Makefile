
SUBDIRS = checkfs fs-tests jittertest ubi-tests

all clean tests: $(SUBDIRS)

.PHONY: $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)
