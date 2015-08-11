#
# Include host/target/compiler selection.
# This will export CC_NAME, MACHINE_NAME, OS_NAME, and HOST_NAME variables.
#
include $(PJDIR)/build.mak

#
# Include global compiler specific definitions
#
include $(PJDIR)/build/cc-$(CC_NAME).mak

#
# (Optionally) Include compiler specific configuration that is
# specific to this project. This configuration file is
# located in this directory.
#
-include cc-$(CC_NAME).mak

#
# Include auto configured compiler specification.
# This will override the compiler settings above.
# Currently this is made OPTIONAL, to prevent people
# from getting errors because they don't re-run ./configure
# after downloading new PJSIP.
#
-include $(PJDIR)/build/cc-auto.mak

#
# Include global machine specific definitions
#
include $(PJDIR)/build/m-$(MACHINE_NAME).mak
-include m-$(MACHINE_NAME).mak

#
# Include target OS specific definitions
#
include $(PJDIR)/build/os-$(OS_NAME).mak

#
# (Optionally) Include target OS specific configuration that is
# specific to this project. This configuration file is
# located in this directory.
#
-include os-$(OS_NAME).mak

#
# Include host specific definitions
#
include $(PJDIR)/build/host-$(HOST_NAME).mak

#
# (Optionally) Include host specific configuration that is
# specific to this project. This configuration file is
# located in this directory.
#
-include host-$(HOST_NAME).mak

#
# Include global user configuration, if any
#
-include $(PJDIR)/user.mak


