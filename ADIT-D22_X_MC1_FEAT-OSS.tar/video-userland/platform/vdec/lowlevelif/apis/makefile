# -----------------------------------------------------------------------------
#        (c) ADIT - Advanced Driver Information Technology JV
# -----------------------------------------------------------------------------
# Title:       Subcomponent makefile
#
# Description: Makefile for the subcomponent: lowlevelif_apis
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# Base settings
# -----------------------------------------------------------------------------

COMP_DIR     = ../..
SUBCOMP_NAME = lowlevelif/apis

# -----------------------------------------------------------------------------
# Component settings
# -----------------------------------------------------------------------------

include $(COMP_DIR)/settings.mk 

# -----------------------------------------------------------------------------
# Sub-Component settings
# -----------------------------------------------------------------------------

INCLUDES     += -Iinc \
                -I$(COMP_DIR)/ADIT_kernel_modules/vid_overlayDriver \
                -I$(COMP_DIR)/ADIT_kernel_modules/vid_debugDriver \
                -I$(KERNEL_PATH)/drivers/video/videantis
LIBRARIES    +=

ARCHIVE      =
PROGRAM      =
SHAREDLIB    = $(LIBRARY_DIR)/libvid_lowlevel.so

PACKAGES     = $(SOURCE_PATH_UL)/platform/system/common \
               $(SOURCE_PATH_GRAPHICS_UL)/platform/svg_common/svg_common \
               $(SOURCE_PATH_GRAPHICS_UL)/platform/svg_resource/svg_resource \
               $(SOURCE_PATH_GRAPHICS_UL)/platform/svg_layer/svg_layer

LINKAGES     = svg-common svg-layer svg-resource pthread

SOURCES      = $(wildcard $(SOURCE_DIR)/*.c)

# -----------------------------------------------------------------------------
# Rules
# -----------------------------------------------------------------------------

all: $(ARCHIVE) $(PROGRAM) $(SHAREDLIB)

install:
	@$(call subcomp_install)

clean \
help \
info:
	-@$(call subcomp_$(MAKECMDGOALS))

# -----------------------------------------------------------------------------
# Include implicite rules
# -----------------------------------------------------------------------------

include $(BASE_DIR)/tools/config/mk/default_tail.mk
