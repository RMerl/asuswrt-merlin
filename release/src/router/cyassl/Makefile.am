SUBDIRS = src ctaocrypt
EXTRA_DIST = cyassl.dsp cyassl.dsw certs/*.pem certs/*.der certs/*.txt \
    lib/dummy cyassl.sln cyassl.vcproj cyassl-iphone.xcodeproj/project.pbxproj \
    doc/*.pdf swig/README swig/*.i swig/cyassl_adds.c swig/*.sh swig/runme.* \
    swig/python_cyassl.vcproj swig/rsasign.py

# !!!! first line of rule has to start with a hard (real) tab, not spaces
basic:
	cd src; $(MAKE); cd ../

openssl-links:
	cd lib; ln -s ../src/.libs/libcyassl.a libcrypto.a; \
    ln -s ../src/.libs/libcyassl.a libssl.a; \
    ln -s ../src/.libs/libcyassl.a libcyassl.a; cd ../

#  !!! test -e with a .name like .libs then a * like *dylib fails so just
#      look for the .dylib on OS X, and .so otherwise but copy all parts
install:
	$(mkinstalldirs) $(DESTDIR)$(includedir) $(DESTDIR)$(libdir); \
    cp -fpR include/* $(DESTDIR)$(includedir); \
    cp -fpR ctaocrypt/include/* $(DESTDIR)$(includedir); \
    cp -fpR src/libcyassl.la $(DESTDIR)$(libdir); \
    if test -e src/.libs/libcyassl.a; then \
    cp -fp src/.libs/libcyassl.a $(DESTDIR)$(libdir); fi; \
    if test -e src/.libs/libcyassl.so; then \
    cp -fpR src/.libs/libcyassl.so* $(DESTDIR)$(libdir); fi; \
    if test -e src/.libs/libcyassl.dylib; then \
    cp -fpR src/.libs/libcyassl.*dylib $(DESTDIR)$(libdir); fi;
