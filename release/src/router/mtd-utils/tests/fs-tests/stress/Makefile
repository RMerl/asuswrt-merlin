
SUBDIRS = atoms

all tests: $(SUBDIRS)

clean: $(SUBDIRS)
	rm -rf run_pdf_test_file_*

.PHONY: $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@ $(MAKECMDGOALS)
