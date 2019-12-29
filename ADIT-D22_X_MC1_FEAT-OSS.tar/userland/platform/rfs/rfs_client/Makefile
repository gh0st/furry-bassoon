
ifeq ($(origin M),undefined)
# =============================================================================

# -----------------------------------------------------------------------------
# Base settings
# -----------------------------------------------------------------------------

COMP_DIR     = ..
SUBCOMP_NAME = rfs_client

# -----------------------------------------------------------------------------
# Component settings
# -----------------------------------------------------------------------------

include $(COMP_DIR)/settings.mk 

# -----------------------------------------------------------------------------
# Sub-Component settings (kernel)
# -----------------------------------------------------------------------------
ifeq ($(MODE),KERNEL)

MODULE_DIR   = $(shell pwd)
MODULE_KO    = rfs_client.ko

all:
	$(MAKE) -C $(KERNEL_PATH) M=$(MODULE_DIR) ARCH=$(TARGET_PROCESSOR) modules

clean:
	@$(MAKE) -C $(KERNEL_PATH) M=$(MODULE_DIR) clean

install:
	@mkdir -p $(DEST_DIR)
	@cp -r $(MODULE_KO) $(DEST_DIR)

help \
info:
	-@$(call subcomp_$(MAKECMDGOALS))

else
# -----------------------------------------------------------------------------
# Sub-Component settings
# -----------------------------------------------------------------------------

INCLUDES     += -Iinclude -Isrc
LIBRARIES    += 

ARCHIVE      =
PROGRAM      =
SHAREDLIB    =

PACKAGES     = $(SOURCE_PATH_UL)/platform/system/common

SOURCES      = $(wildcard $(SOURCE_DIR)/um/*.c)

LINKAGES     =

# -----------------------------------------------------------------------------
# Rules
# -----------------------------------------------------------------------------

all: $(SHAREDLIB)
	@$(CP) $(SHAREDLIB) .

clean:
	-@$(call subcomp_clean)

install \
help \
info:
	-@$(call subcomp_$(MAKECMDGOALS))
	@echo $(TOPDIR)

# -----------------------------------------------------------------------------
# Include implicite rules
# -----------------------------------------------------------------------------

include ${BASE_DIR}/tools/config/mk/default_tail.mk

endif
# =============================================================================
else
include $(M)/Makefile.kernel
endif
