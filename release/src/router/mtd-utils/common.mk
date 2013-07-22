# Stolen from Linux build system
comma = ,
try-run = $(shell set -e; ($(1)) >/dev/null 2>&1 && echo "$(2)" || echo "$(3)")
cc-option = $(call try-run, $(CC) $(1) -c -xc /dev/null -o /dev/null,$(1),$(2))

CFLAGS ?= -O2 -g
WFLAGS := -Wall \
	$(call cc-option,-Wextra) \
	$(call cc-option,-Wwrite-strings) \
	$(call cc-option,-Wno-sign-compare)
CFLAGS += $(WFLAGS)
#SECTION_CFLAGS := $(call cc-option,-ffunction-sections -fdata-sections -Wl$(comma)--gc-sections)
CFLAGS += $(SECTION_CFLAGS)

ifneq ($(WITHOUT_LARGEFILE), 1)
  CPPFLAGS += -D_FILE_OFFSET_BITS=64
endif

DESTDIR?=
PREFIX=/usr
EXEC_PREFIX=$(PREFIX)
SBINDIR=$(EXEC_PREFIX)/sbin
MANDIR=$(PREFIX)/share/man
INCLUDEDIR=$(PREFIX)/include

ifndef BUILDDIR
ifeq ($(origin CROSS),undefined)
  BUILDDIR := $(CURDIR)
else
# Remove the trailing slash to make the directory name
  BUILDDIR := $(CURDIR)/$(CROSS:-=)
endif
endif
override BUILDDIR := $(patsubst %/,%,$(BUILDDIR))

override TARGETS := $(addprefix $(BUILDDIR)/,$(TARGETS))

ifeq ($(V),1)
XECHO = @:
XPRINTF = @:
Q =
else
XECHO = @echo
XPRINTF = @printf
Q = @
endif
define BECHO
$(XPRINTF) '  %-7s %s\n' "$1" "$(subst $(BUILDDIR)/,,$@)"
endef

all:: $(TARGETS)

clean::
	rm -f $(BUILDDIR)/*.o $(TARGETS) $(BUILDDIR)/.*.c.dep

install:: $(TARGETS)

define _mkdep
$(BUILDDIR)/$1$2: $(addprefix $(BUILDDIR)/$1,$(obj-$2) $3) $(addprefix $(BUILDDIR)/,$4)
endef
define mkdep
$(call _mkdep,$1,$2,$3 $2.o,$4 lib/libmtd.a)
endef

%: %.o $(LDDEPS)
	$(call BECHO,LD)
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) $(LDFLAGS_$(notdir $@)) -g -o $@ $^ $(LDLIBS) $(LDLIBS_$(notdir $@))

$(BUILDDIR)/%.a:
	$(call BECHO,AR)
	$(Q)$(AR) cr $@ $^
	$(Q)$(RANLIB) $@

$(BUILDDIR)/%.o: %.c $(OBJDEPS)
ifneq ($(BUILDDIR),$(CURDIR))
	$(Q)mkdir -p $(dir $@)
endif
	$(call BECHO,CC)
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $< -g -Wp,-MD,$(BUILDDIR)/.$(<F).dep

.SUFFIXES:

IGNORE=${wildcard $(BUILDDIR)/.*.c.dep}
-include ${IGNORE}

PHONY += all clean install
.PHONY: $(PHONY)
