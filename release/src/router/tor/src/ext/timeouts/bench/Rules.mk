BENCH_MODS = bench.so $(BENCH_ALGOS:%=bench-%.so)
BENCH_ALGOS = wheel heap llrb
BENCH_OPS = add del expire

$(top_builddir)/bench/bench.so: $(top_srcdir)/bench/bench.c
$(top_builddir)/bench/bench-wheel.so: $(top_srcdir)/bench/bench-wheel.c
$(top_builddir)/bench/bench-heap.so: $(top_srcdir)/bench/bench-heap.c
$(top_builddir)/bench/bench-llrb.so: $(top_srcdir)/bench/bench-llrb.c

$(BENCH_MODS:%=$(top_builddir)/bench/%): $(top_srcdir)/timeout.h $(top_srcdir)/timeout.c $(top_srcdir)/bench/bench.h
	mkdir -p $(@D)
	@$(SHRC); echo_cmd $(CC) -o $@ $(top_srcdir)/bench/$(@F:%.so=%.c) $(ALL_CPPFLAGS) $(ALL_CFLAGS) $(ALL_SOFLAGS) $(ALL_LDFLAGS) $(ALL_LIBS)

$(BENCH_OPS:%=$(top_builddir)/bench/wheel-%.dat): $(top_builddir)/bench/bench-wheel.so $(top_builddir)/bench/bench.so $(top_srcdir)/bench/bench-aux.lua
$(BENCH_OPS:%=$(top_builddir)/bench/heap-%.dat): $(top_builddir)/bench/bench-heap.so $(top_builddir)/bench/bench.so $(top_srcdir)/bench/bench-aux.lua
$(BENCH_OPS:%=$(top_builddir)/bench/llrb-%.dat): $(top_builddir)/bench/bench-llrb.so $(top_builddir)/bench/bench.so $(top_srcdir)/bench/bench-aux.lua

$(BENCH_ALGOS:%=$(top_builddir)/bench/%-add.dat): $(top_srcdir)/bench/bench-add.lua
	@$(SHRC); echo_cmd cd $(@D) && echo_cmd $(LUA) $${top_srcdir}/bench/bench-add.lua $${top_builddir}/bench/bench-$(@F:%-add.dat=%).so > $(@F).tmp
	mv $@.tmp $@

$(BENCH_ALGOS:%=$(top_builddir)/bench/%-del.dat): $(top_srcdir)/bench/bench-del.lua
	@$(SHRC); echo_cmd cd $(@D) && echo_cmd $(LUA) $${top_srcdir}/bench/bench-del.lua $${top_builddir}/bench/bench-$(@F:%-del.dat=%).so > $(@F).tmp
	mv $@.tmp $@

$(BENCH_ALGOS:%=$(top_builddir)/bench/%-expire.dat): $(top_srcdir)/bench/bench-expire.lua
	@$(SHRC); echo_cmd cd $(@D) && echo_cmd $(LUA) $${top_srcdir}/bench/bench-expire.lua $${top_builddir}/bench/bench-$(@F:%-expire.dat=%).so > $(@F).tmp
	mv $@.tmp $@

$(top_builddir)/bench/bench.eps: \
	$(BENCH_OPS:%=$(top_builddir)/bench/wheel-%.dat) \
	$(BENCH_OPS:%=$(top_builddir)/bench/heap-%.dat)
#	$(BENCH_OPS:%=$(top_builddir)/bench/llrb-%.dat)

$(top_builddir)/bench/bench.eps: $(top_srcdir)/bench/bench.plt
	@$(SHRC); echo_cmd cd $(@D) && echo_cmd gnuplot $${top_srcdir}/bench/bench.plt > $(@F).tmp
	mv $@.tmp $@

$(top_builddir)/bench/bench.pdf: $(top_builddir)/bench/bench.eps
	@$(SHRC); echo_cmd ps2pdf $${top_builddir}/bench/bench.eps $@

bench-mods: $(BENCH_MODS:%=$(top_builddir)/bench/%)

bench-all: $(top_builddir)/bench/bench.pdf

bench-clean:
	$(RM) -r $(top_builddir)/bench/*.so $(top_builddir)/bench/*.dSYM
	$(RM) $(top_builddir)/bench/*.dat $(top_builddir)/bench/*.tmp
	$(RM) $(top_builddir)/bench/bench.{eps,pdf}
