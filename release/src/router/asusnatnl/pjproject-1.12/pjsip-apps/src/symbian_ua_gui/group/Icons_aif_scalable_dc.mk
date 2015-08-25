# ============================================================================
#  Name	 : Icons_aif_scalable_dc.mk
#  Part of  : symbian_ua_gui
#
#  Description:
# 
# ============================================================================


ifeq (WINS,$(findstring WINS, $(PLATFORM)))
ZDIR=$(EPOCROOT)epoc32\release\$(PLATFORM)\$(CFG)\Z
else
ZDIR=$(EPOCROOT)epoc32\data\z
endif


# ----------------------------------------------------------------------------
# TODO: Configure these
# ----------------------------------------------------------------------------

TARGETDIR=$(ZDIR)\resource\apps
ICONTARGETFILENAME=$(TARGETDIR)\symbian_ua_gui_aif.mif
HEADERDIR=$(EPOCROOT)epoc32\include
HEADERFILENAME=$(HEADERDIR)\symbian_ua_gui_aif.mbg

ICONDIR=..\gfx

do_nothing :
	@rem do_nothing

MAKMAKE : do_nothing

BLD : do_nothing

CLEAN : do_nothing

LIB : do_nothing

CLEANLIB : do_nothing

# ----------------------------------------------------------------------------
# TODO: Configure these.
#
# NOTE 1: DO NOT DEFINE MASK FILE NAMES! They are included automatically by
# MifConv if the mask detph is defined.
#
# NOTE 2: Usually, source paths should not be included in the bitmap
# definitions. MifConv searches for the icons in all icon directories in a
# predefined order, which is currently \s60\icons, \s60\bitmaps2.
# The directory \s60\icons is included in the search only if the feature flag
# __SCALABLE_ICONS is defined.
# ----------------------------------------------------------------------------
# NOTE: if you have JUSTINTIME enabled for your S60 3rd FP1 or newer SDK
# and this command crashes, consider adding "/X" to the command line.
# See <http://forum.nokia.com/document/Forum_Nokia_Technical_Library_v1_35/contents/FNTL/Build_process_fails_at_mif_file_creation_in_S60_3rd_Ed_FP1_SDK.htm>
# ----------------------------------------------------------------------------

RESOURCE :	
	mifconv $(ICONTARGETFILENAME) \
		/H$(HEADERFILENAME) \
		/c32,8 $(ICONDIR)\qgn_menu_symbian_ua_gui.svg
		
FREEZE : do_nothing

SAVESPACE : do_nothing

RELEASABLES :
	@echo $(ICONTARGETFILENAME)

FINAL : do_nothing

