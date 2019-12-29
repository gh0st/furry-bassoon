# -----------------------------------------------------------------------------
#        (c) ADIT - Advanced Driver Information Technology JV
# -----------------------------------------------------------------------------
# Title:       Subcomponent makefile
#
# Description: Makefile for the subcomponent: protocol
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# Base settings
# -----------------------------------------------------------------------------

COMP_DIR     = ..
SUBCOMP_NAME = hid_com_server

# -----------------------------------------------------------------------------
# Component settings
# -----------------------------------------------------------------------------

SOURCE_DIR    = .
include $(COMP_DIR)/settings.mk 
MV_STAGING_FOLDER = ${MV_PROJECT_FOLDER}/tmp/staging

# -----------------------------------------------------------------------------
# Sub-Component settings
# -----------------------------------------------------------------------------

INCLUDES     += -I$(MV_STAGING_FOLDER)/armv6-mv-linux/usr/include/ -I$(SOURCE_PATH_UL)/platform/ipod_ctrl/hid_com_server/include -I$(SOURCE_PATH_UL)/platform/ipod_ctrl/transport/include/ -I$(SOURCE_PATH_UL)/platform/ipod_ctrl/protocol/include/
LIBRARIES    += -lpthread -lrt
LIBRARIES    += -L$(MV_STAGING_FOLDER)/armv6-mv-linux/usr/lib
ARCHIVE      = 
PROGRAM      = $(BINARY_DIR)/hid_com
SHAREDLIB    =

PACKAGES     = $(SOURCE_PATH_UL)/platform/system/common

SOURCES      = $(SOURCE_DIR)/src/hid_com_server.c

# -----------------------------------------------------------------------------
# Rules
# -----------------------------------------------------------------------------

all: $(ARCHIVE) $(PROGRAM) $(SHAREDLIB)

install:
	@mkdir -p $(STAGING_PATH)/etc/init.d
	install -m 0755 hid_com-init.sh --target-directory=$(STAGING_PATH)/etc/init.d
	@mkdir -p $(STAGING_PATH)/etc/rc5.d
	( cd $(STAGING_PATH)/etc/rc5.d; ln -s ../init.d/hid_com-init.sh S26hid_com-init; )
	@$(call subcomp_install)

clean \
help \
info:
	-@$(call subcomp_$(MAKECMDGOALS))

# -----------------------------------------------------------------------------
# Include implicite rules
# -----------------------------------------------------------------------------

include ${BASE_DIR}/tools/config/mk/default_tail.mk
