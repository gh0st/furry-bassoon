# -----------------------------------------------------------------------------
#        (c) ADIT - Advanced Driver Information Technology JV
# -----------------------------------------------------------------------------
# Title:       Subcomponent makefile
#
# Description: Makefile for the subcomponent: vc1_dec
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# Base settings
# -----------------------------------------------------------------------------

COMP_DIR     = ..
CODEC_NAME   = vc1
SUBCOMP_NAME = v$(CODEC_NAME)_linux

# -----------------------------------------------------------------------------
# Component settings
# -----------------------------------------------------------------------------

include $(COMP_DIR)/settings.mk 

# -----------------------------------------------------------------------------
# Sub-Component settings
# -----------------------------------------------------------------------------

INCLUDES     += -I$(SOURCE_DIR) -Iinclude \
                -I$(COMP_DIR)/lowlevelif/apis/inc
LIBRARIES    +=

ARCHIVE      =
PROGRAM      =
SHAREDLIB    = $(LIBRARY_DIR)/libvid_$(CODEC_NAME)_dec.so

PACKAGES     = $(SOURCE_PATH_VIDEO_UL)/platform/vdec/ADIT_fbmem_allocator_svg \
               $(SOURCE_PATH_UL)/platform/system/common \
               $(SOURCE_PATH_GRAPHICS_UL)/platform/svg_common/svg_common \
               $(SOURCE_PATH_GRAPHICS_UL)/platform/svg_resource/svg_resource \
               $(SOURCE_PATH_GRAPHICS_UL)/platform/svg_layer/svg_layer

LINKAGES     = svg-common svg-layer svg-resource pthread

SOURCES      = $(wildcard $(SOURCE_DIR)/*.c)

# -----------------------------------------------------------------------------

TESTSCR      = test/v$(CODEC_NAME)_decoder_app.c
TESTLIBS     = -lvid_$(CODEC_NAME)_dec -lvid_lowlevel -lvmp_fbmem_allocator
TESTAPP      = $(BINARY_DIR)/v$(CODEC_NAME)_decoder_app

# -----------------------------------------------------------------------------
# Rules
# -----------------------------------------------------------------------------

all: $(ARCHIVE) $(PROGRAM) $(SHAREDLIB) $(TESTAPP)

install:
	@$(call subcomp_install)
	$(CP) $(TESTAPP) $(STAGING_PATH_BIN)/

clean:
	-@$(call subcomp_clean)
	-@$(RM) $(TESTAPP)

help \
info:
	-@$(call subcomp_$(MAKECMDGOALS))

$(TESTAPP): $(SHAREDLIB) $(TESTSCR)
	@$(call info_rule_head,LD,"Make program $@")
	$(Q)$(MD) $(dir $@)
	$(Q)$(LD) $(CFLAGS_C_ARG) $(LDFLAGS_ARG) $(TESTLIBS) -o $(TESTAPP) $(TESTSCR)
	@$(call info_rule_tail)

# -----------------------------------------------------------------------------
# Include implicite rules
# -----------------------------------------------------------------------------

include $(BASE_DIR)/tools/config/mk/default_tail.mk
