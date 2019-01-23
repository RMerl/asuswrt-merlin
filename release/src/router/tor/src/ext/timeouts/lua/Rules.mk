$(LUA_APIS:%=$(top_builddir)/lua/%/timeout.so): $(top_srcdir)/lua/timeout-lua.c $(top_srcdir)/timeout.h $(top_srcdir)/timeout.c
	mkdir -p $(@D)
	@$(SHRC); echo_cmd $(CC) -o $@ $(top_srcdir)/lua/timeout-lua.c -I$(top_srcdir) -DWHEEL_BIT=$(WHEEL_BIT) -DWHEEL_NUM=$(WHEEL_NUM) $(LUA53_CPPFLAGS) $(ALL_CPPFLAGS) $(ALL_CFLAGS) $(ALL_SOFLAGS) $(ALL_LDFLAGS) $(ALL_LIBS)

$(top_builddir)/lua/5.1/timeouts.so: $(top_builddir)/lua/5.1/timeout.so
$(top_builddir)/lua/5.2/timeouts.so: $(top_builddir)/lua/5.2/timeout.so
$(top_builddir)/lua/5.3/timeouts.so: $(top_builddir)/lua/5.3/timeout.so

$(LUA_APIS:%=$(top_builddir)/lua/%/timeouts.so):
	cd $(@D) && ln -fs timeout.so timeouts.so

lua-5.1: $(top_builddir)/lua/5.1/timeout.so $(top_builddir)/lua/5.1/timeouts.so
lua-5.2: $(top_builddir)/lua/5.2/timeout.so $(top_builddir)/lua/5.2/timeouts.so
lua-5.3: $(top_builddir)/lua/5.3/timeout.so $(top_builddir)/lua/5.3/timeouts.so

lua-clean:
	$(RM) -r $(top_builddir)/lua/5.?

clean: lua-clean

