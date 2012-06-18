all:
	$(MAKE) -C src

install:
	$(MAKE) -C src romfs
	install -d $(INSTALLDIR)/rom/etc
	cp -rf admin-root $(INSTALLDIR)/rom/etc/web 

clean:
	$(MAKE) -C src clean
