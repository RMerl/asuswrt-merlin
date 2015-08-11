ifeq ($(LIBDIR),)
LIBDIR = ../lib
endif
ifeq ($(BINDIR),)
BINDIR = ../bin
endif

#
# The full path of output lib file (e.g. ../lib/libapp.a).
#
LIB = $($(APP)_LIB)

#
# The full path of output executable file (e.g. ../bin/app.exe).
#
EXE = $($(APP)_EXE)

#
# Source directory
#
SRCDIR = $($(APP)_SRCDIR)

#
# Output directory for object files (i.e. output/target)
#
OBJDIR = output/$(app)-$(TARGET_NAME)

ifeq ($(OS_NAME),linux-kernel)
export $(APP)_CFLAGS += -DKBUILD_MODNAME=$(app) -DKBUILD_BASENAME=$(app)
endif


#
# OBJS is ./output/target/file.o
#
OBJS = $(foreach file, $($(APP)_OBJS), $(OBJDIR)/$(file))
OBJDIRS := $(sort $(dir $(OBJS)))

#
# FULL_SRCS is ../src/app/file1.c ../src/app/file1.S
#
FULL_SRCS = $(foreach file, $($(APP)_OBJS), $(SRCDIR)/$(basename $(file)).c $(SRCDIR)/$(basename $(file)).cpp $(SRCDIR)/$(basename $(file)).S)

#
# When generating dependency (gcc -MM), ideally we use only either
# CFLAGS or CXXFLAGS (not both). But I just couldn't make if/ifeq to work.
#
DEPFLAGS = $($(APP)_CXXFLAGS) $($(APP)_CFLAGS)

# Dependency file
DEP_FILE := .$(app)-$(TARGET_NAME).depend


print_common:
	@echo "###"
	@echo "### DUMPING MAKE VARIABLES (I WON'T DO ANYTHING ELSE):"
	@echo "###"
	@echo APP=$(APP)
	@echo OBJDIR=$(OBJDIR)
	@echo OBJDIRS=$(OBJDIRS)
	@echo OBJS=$(OBJS)
	@echo SRCDIR=$(SRCDIR)
	@echo FULL_SRCS=$(FULL_SRCS)
	@echo $(APP)_CFLAGS=$($(APP)_CFLAGS)
	@echo $(APP)_CXXFLAGS=$($(APP)_CXXFLAGS)
	@echo $(APP)_LDFLAGS=$($(APP)_LDFLAGS)
	@echo DEPFLAGS=$(DEPFLAGS)
	@echo CC=$(CC)
	@echo AR=$(AR)
	@echo RANLIB=$(RANLIB)

print_bin: print_common
	@echo EXE=$(EXE)
	@echo BINDIR=$(BINDIR)

print_lib: print_common
	@echo LIB=$(LIB)
	@echo LIBDIR=$(LIBDIR)

$(LIB): $(OBJDIRS) $(OBJS) $($(APP)_EXTRA_DEP)
	if test ! -d $(LIBDIR); then $(subst @@,$(subst /,$(HOST_PSEP),$(LIBDIR)),$(HOST_MKDIR)); fi
	$(AR) $(LIB) $(OBJS)
	$(RANLIB) $(LIB)

$(EXE): $(OBJDIRS) $(OBJS) $($(APP)_EXTRA_DEP)
	if test ! -d $(BINDIR); then $(subst @@,$(subst /,$(HOST_PSEP),$(BINDIR)),$(HOST_MKDIR)); fi
	$(LD) $(LDOUT)$(subst /,$(HOST_PSEP),$(EXE)) \
	    $(subst /,$(HOST_PSEP),$(OBJS)) $($(APP)_LDFLAGS)

$(OBJDIR)/$(app).o: $(OBJDIRS) $(OBJS)
	$(CROSS_COMPILE)ld -r -o $@ $(OBJS)

$(OBJDIR)/$(app).ko: $(OBJDIR)/$(app).o
	@echo Creating kbuild Makefile...
	@echo "# Our module name:" > $(OBJDIR)/Makefile
	@echo 'obj-m += $(app).o' >> $(OBJDIR)/Makefile
	@echo >> $(OBJDIR)/Makefile
	@echo "# Object members:" >> $(OBJDIR)/Makefile
	@echo -n '$(app)-objs += ' >> $(OBJDIR)/Makefile
	@for file in $($(APP)_OBJS); do \
		echo -n "$$file " >> $(OBJDIR)/Makefile; \
	done
	@echo >> $(OBJDIR)/Makefile
	@echo >> $(OBJDIR)/Makefile
	@echo "# Prevent .o files to be built by kbuild:" >> $(OBJDIR)/Makefile
	@for file in $($(APP)_OBJS); do \
		echo ".PHONY: `pwd`/$(OBJDIR)/$$file" >> $(OBJDIR)/Makefile; \
	done
	@echo >> $(OBJDIR)/Makefile
	@echo all: >> $(OBJDIR)/Makefile
	@echo -e "\tmake -C $(KERNEL_DIR) M=`pwd`/$(OBJDIR) modules $(KERNEL_ARCH)" >> $(OBJDIR)/Makefile
	@echo Invoking kbuild...
	make -C $(OBJDIR)

../lib/$(app).ko: $(LIB) $(OBJDIR)/$(app).ko
	cp $(OBJDIR)/$(app).ko ../lib

$(OBJDIR)/%$(OBJEXT): $(SRCDIR)/%.m
	$(CC) $($(APP)_CFLAGS) \
		$(CC_OUT)$(subst /,$(HOST_PSEP),$@) \
		$(subst /,$(HOST_PSEP),$<) 

$(OBJDIR)/%$(OBJEXT): $(SRCDIR)/%.c
	$(CC) $($(APP)_CFLAGS) \
		$(CC_OUT)$(subst /,$(HOST_PSEP),$@) \
		$(subst /,$(HOST_PSEP),$<) 

$(OBJDIR)/%$(OBJEXT): $(SRCDIR)/%.S
	$(CC) $($(APP)_CFLAGS) \
		$(CC_OUT)$(subst /,$(HOST_PSEP),$@) \
		$(subst /,$(HOST_PSEP),$<) 

$(OBJDIR)/%$(OBJEXT): $(SRCDIR)/%.cpp
	$(CC) $($(APP)_CXXFLAGS) \
		$(CC_OUT)$(subst /,$(HOST_PSEP),$@) \
		$(subst /,$(HOST_PSEP),$<)

$(OBJDIRS):
	$(subst @@,$(subst /,$(HOST_PSEP),$@),$(HOST_MKDIR)) 

$(LIBDIR):
	$(subst @@,$(subst /,$(HOST_PSEP),$(LIBDIR)),$(HOST_MKDIR))

$(BINDIR):
	$(subst @@,$(subst /,$(HOST_PSEP),$(BINDIR)),$(HOST_MKDIR))

clean:
	$(subst @@,$(subst /,$(HOST_PSEP),$(OBJDIR)/*),$(HOST_RMR))
	$(subst @@,$(subst /,$(HOST_PSEP),$(OBJDIR)),$(HOST_RMDIR))
ifeq ($(OS_NAME),linux-kernel)
	rm -f ../lib/$(app).o
endif

gcov-report:
	for file in $(FULL_SRCS); do \
		gcov $$file -n -o $(OBJDIR); \
	done

realclean: clean
	$(subst @@,$(subst /,$(HOST_PSEP),$(LIB)) $(subst /,$(HOST_PSEP),$(EXE)),$(HOST_RM))
	$(subst @@,$(DEP_FILE),$(HOST_RM))
ifeq ($(OS_NAME),linux-kernel)
	rm -f ../lib/$(app).ko
endif

depend:
	$(subst @@,$(DEP_FILE),$(HOST_RM))
	for F in $(FULL_SRCS); do \
	   if test -f $$F; then \
	     echo "$(OBJDIR)/" | tr -d '\n' >> $(DEP_FILE); \
	     if $(CC) -M $(DEPFLAGS) $$F | sed '/^#/d' >> $(DEP_FILE); then \
		true; \
	     else \
		echo 'err:' >> $(DEP_FILE); \
		rm -f $(DEP_FILE); \
		exit 1; \
	     fi; \
	   fi; \
	done;

dep: depend

-include $(DEP_FILE)

