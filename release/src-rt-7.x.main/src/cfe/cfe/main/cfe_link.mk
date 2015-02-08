#
# This Makefile snippet takes care of linking the firmware.
#

pci : $(PCICOMMON) $(PCIMACHDEP)
	echo done

.PHONY: $(LDSCRIPT)

$(LDSCRIPT) : $(LDSCRIPT_TEMPLATE) 
	$(GCPP) -P $(GENLDFLAGS) $(LDSCRIPT_TEMPLATE) $(LDSCRIPT)

cfe cfe.bin : $(CRT0OBJS) $(BSPOBJS) $(LIBCFE) cfe.lds
	$(GLD) -o cfe -Map cfe.map $(LDFLAGS) $(CRT0OBJS) $(BSPOBJS) -L. -lcfe $(LDLIBS)
	$(OBJDUMP) -d cfe > cfe.dis
	$(OBJCOPY) -O binary -R .reginfo -R .note -R .comment -R .mdebug -S cfe cfe.bin
	$(OBJCOPY) --input-target=binary --output-target=srec cfe.bin cfe.srec
ifdef CFE_MAXSIZE
	@cfe_size=$$(stat -c %s cfe.bin) && \
	test $$cfe_size -le $(CFE_MAXSIZE) || { \
	    if [[ -n "$(CFE_INTERNAL)" ]]; then \
		echo "WARNING: cfe.bin bootrom image size: $$cfe_size exceeds limit: $(CFE_MAXSIZE)" >&2; \
	    else \
		echo "ERROR: cfe.bin bootrom image size: $$cfe_size exceeds limit: $(CFE_MAXSIZE)" >&2; \
		exit 2; \
	    fi; \
	}
endif

cfe.mem: cfe.bin
	$(OBJCOPY) --input-target=binary --output-target=swapmif cfe.bin cfe.mem

cfe.qt: cfe
	$(OBJCOPY) --output-target=swapqt --addr-bits 20 --qt-len 1 cfe cfe.qt

cfe.flash : cfe.bin mkflashimage
	./mkflashimage -v ${ENDIAN} -B ${CFG_BOARDNAME} -V ${CFE_VER_MAJ}.${CFE_VER_MIN}.${CFE_VER_ECO} cfe.bin cfe.flash
	$(OBJCOPY) --input-target=binary --output-target=srec cfe.flash cfe.flash.srec


clean :
	rm -f *.o *~ cfe cfe.bin cfe.dis cfe.map cfe.srec cfe.lds
	rm -f makereg ${CPU}_socregs.inc mkpcidb pcidevs_data2.h mkflashimage
	rm -f build_date.c
	rm -f libcfe.a
	rm -f cfe.flash cfe.flash.srec $(CLEANOBJS)

distclean : clean
