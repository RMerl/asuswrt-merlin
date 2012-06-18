EXTRA_DIST = doxygen.cfg.in

docs: doxygen.cfg
	doxygen $^

docs-upload: docs
	ln -s html api-1.0
	rsync -av api-1.0/ dsd_,libusb@web.sourceforge.net:htdocs/api-1.0/
	rm -f api-1.0

