# vim:syntax=make

ifeq ($(V),)
define quiet
	@
endef
endif

# $(1) --> $(2) w/o any message.
# $(1):	path to source file
# $(2): path to destination file
define __inst
	$(quiet)install -D $(1) $(2)
endef

# $(1) --> $(2)
# $(1):	path to source file
# $(2): path to destination file
define _inst
	@if [ -z "$(V)" ] ; then echo "  [INSTALL]  $(notdir $(1)) -> $(notdir $(2))" ; fi
	$(quiet)install -D $(1) $(2)
endef

# ln -sf $(1) $(2)
# $(1):	relative path to source file
# $(2): path to destination file
define _ln
	@if [ -z "$(V)" ] ; then echo "  [LN]  $(notdir $(2)) -> $(notdir $(1))" ; fi
	$(quiet)ln -sf $(1) $(2)
endef

# foo/*.so --> *.so.l
# $(1): source directory
# $(2): destination directory
# #(3): source filename. relative path may included.
define inst_so
	@if [ -z "$(V)" ] ; then echo "  [INSTALL]  $(3) -> $(addsuffix .l,$(notdir $(3)))" ; fi
	$(call __inst,$(1)/$(3),$(2)/$(addsuffix .l,$(notdir $(3))))
endef

# foo/*.o --> *.obj
# $(1): source directory
# $(2): destination directory
# #(3): source filename. relative path may included.
define inst_obj
	@if [ -z "$(V)" ] ; then echo "  [INSTALL]  $(3) -> $(addsuffix .obj,$(basename $(notdir $(3))))" ; fi
	$(call __inst,$(1)/$(3),$(2)/$(addsuffix .obj,$(basename $(notdir $(3)))))
endef

# foo/* --> *
# $(1): source directory
# $(2): destination directory
# #(3): source filename. relative path may included.
define inst
	@if [ -z "$(V)" ] ; then echo "  [INSTALL]  $(3) -> $(notdir $(3))" ; fi
	$(call __inst,$(1)/$(3),$(2)/$(notdir $(3)))
endef
