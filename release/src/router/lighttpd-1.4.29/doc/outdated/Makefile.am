DOCS=accesslog.txt \
authentication.txt \
cgi.txt \
compress.txt \
configuration.txt \
extforward.txt \
fastcgi-state.txt \
fastcgi.txt \
features.txt \
performance.txt \
plugins.txt \
proxy.txt \
redirect.txt \
rewrite.txt \
secdownload.txt \
security.txt \
simple-vhost.txt \
skeleton.txt \
ssi.txt \
ssl.txt \
state.txt \
rrdtool.txt \
alias.txt \
userdir.txt \
mysqlvhost.txt \
access.txt \
traffic-shaping.txt \
setenv.txt \
status.txt \
scgi.txt \
cml.txt \
trigger_b4_dl.txt \
webdav.txt \
expire.txt \
dirlisting.txt \
evhost.txt \
magnet.txt

HTMLDOCS=accesslog.html \
	 authentication.html \
	 cgi.html \
	 compress.html \
	 configuration.html \
	 extforward.html \
	 fastcgi-state.html \
	 fastcgi.html \
	 features.html \
	 performance.html \
	 plugins.html \
	 proxy.html \
	 redirect.html \
	 rewrite.html \
	 secdownload.html \
	 security.html \
	 simple-vhost.html \
	 skeleton.html \
	 ssi.html \
	 ssl.html \
	 state.html \
	 rrdtool.html \
	 alias.html \
	 userdir.html \
	 mysqlvhost.html \
	 access.html \
	 traffic-shaping.html \
	 setenv.html \
	 status.html \
	 scgi.html \
	 cml.html \
	 trigger_b4_dl.html \
	 webdav.html \
	 expire.html \
	 dirlisting.html \
	 evhost.html \
	 magnet.html

EXTRA_DIST= \
	state.dot fastcgi-state.dot \
	$(DOCS)

.txt.html:
	rst2html $^ > $@


html-local: $(HTMLDOCS)

#%.ps.gz: %.ps
#	gzip $^

#%.ps: %.dot
#	dot -Tps -o $@ $^

clean-local:
	rm -f *.html


