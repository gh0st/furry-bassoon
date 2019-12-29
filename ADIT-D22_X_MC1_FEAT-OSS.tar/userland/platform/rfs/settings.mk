# -----------------------------------------------------------------------------
#        (c) ADIT - Advanced Driver Information Technology JV
# -----------------------------------------------------------------------------
# Title:       Component Globals default makefile 'settings.mk'
#
# Description: This is the default makefile which contains
#              all basic settings needed for the component.
# -----------------------------------------------------------------------------
# Requirement: COMP_DIR, SUBCOMP_NAME
# -----------------------------------------------------------------------------

COMPONENT     = RFS
COMP_NAME     = platform/rfs

# -----------------------------------------------------------------------------
# Base Directory
# -----------------------------------------------------------------------------

BASE_DIR      = $(COMP_DIR)/../../..

# -----------------------------------------------------------------------------
# Component Settings
# -----------------------------------------------------------------------------

C_FLAGS       = -fPIC
CPP_FLAGS     =
AR_FLAGS      = rcs
LD_FLAGS      = -L$(LIBRARY_DIR) $(LIBRARIES)
SO_FLAGS      = -L$(LIBRARY_DIR) $(LIBRARIES)

C_DEFINES     = -DPIC
CPP_DEFINES   = $(C_DEFINES)

# -----------------------------------------------------------------------------
# Include all global settings
# -----------------------------------------------------------------------------

include $(BASE_DIR)/tools/config/mk/default.mk

# -----------------------------------------------------------------------------
# Export settings for kernel module
# -----------------------------------------------------------------------------

ifeq ($(MODE),KERNEL)
export PATH := $(PATH):$(TOOLCHAIN_$(ARCH))
export CROSS_COMPILE
endif
