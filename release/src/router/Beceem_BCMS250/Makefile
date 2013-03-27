DIR_NAME=Beceem_BCMS250

ifneq ($(wildcard ./src),)
all:
	@echo " [$(DIR_NAME)] CC"
	make -C src

install:
	@echo " [$(DIR_NAME)] Installing to $(INSTALLDIR)"
	make -C src install

clean:
	make -C src clean

else
all:
	@echo " [$(DIR_NAME)] CC"
	rm -rf $(INSTALLDIR)/$(DIR_NAME)

install:
	@echo " [$(DIR_NAME)] Installing to $(INSTALLDIR)"
	mkdir -p $(INSTALLDIR)
	cp -rf prebuild/lib $(INSTALLDIR)/
	cp -rf prebuild/rom $(INSTALLDIR)/
	cp -rf prebuild/sbin $(INSTALLDIR)/

clean:
	echo "$(INSTALLDIR)"
	rm -rf $(INSTALLDIR)/$(DIR_NAME)
endif

