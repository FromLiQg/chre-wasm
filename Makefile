#
# CHRE Makefile
#

# Environment Setup ############################################################

# Building CHRE is always done in-tree so the CHRE_PREFIX can be assigned to the
# current directory.
CHRE_PREFIX = .

# Environment Checks ###########################################################

# Ensure that the user has specified a path to the SLPI tree which is required
# build the runtime.
ifeq ($(SLPI_PREFIX),)
$(error "You must supply an SLPI_PREFIX environment variable \
         containing a path to the SLPI source tree. Example: \
         export SLPI_PREFIX=$$HOME/slpi_proc")
endif

# Build Configuration ##########################################################

OUTPUT_NAME = libchre

# Include Paths ################################################################

# Hexagon Include Paths
HEXAGON_CFLAGS += -I$(SLPI_PREFIX)/build/ms
HEXAGON_CFLAGS += -I$(SLPI_PREFIX)/build/cust
HEXAGON_CFLAGS += -I$(SLPI_PREFIX)/core/api/debugtools
HEXAGON_CFLAGS += -I$(SLPI_PREFIX)/core/api/services
HEXAGON_CFLAGS += -I$(SLPI_PREFIX)/core/api/kernel/devcfg
HEXAGON_CFLAGS += -I$(SLPI_PREFIX)/core/api/kernel/qurt
HEXAGON_CFLAGS += -I$(SLPI_PREFIX)/core/api/dal
HEXAGON_CFLAGS += -I$(SLPI_PREFIX)/core/api/mproc
HEXAGON_CFLAGS += -I$(SLPI_PREFIX)/core/api/systemdrivers
HEXAGON_CFLAGS += -I$(SLPI_PREFIX)/platform/inc
HEXAGON_CFLAGS += -I$(SLPI_PREFIX)/platform/inc/HAP
HEXAGON_CFLAGS += -I$(SLPI_PREFIX)/platform/inc/stddef
HEXAGON_CFLAGS += -I$(SLPI_PREFIX)/platform/rtld/inc
HEXAGON_CFLAGS += -I$(SLPI_PREFIX)/Sensors/api
HEXAGON_CFLAGS += -I$(SLPI_PREFIX)/Sensors/common/idl/inc
HEXAGON_CFLAGS += -I$(SLPI_PREFIX)/Sensors/common/util/mathtools/inc

# Compiler Flags ###############################################################

# Symbols required by the runtime for conditional compilation.
COMMON_CFLAGS += -DCHRE_MINIMUM_LOG_LEVEL=CHRE_LOG_LEVEL_DEBUG
COMMON_CFLAGS += -DNANOAPP_MINIMUM_LOG_LEVEL=CHRE_LOG_LEVEL_DEBUG
COMMON_CFLAGS += -DCHRE_ASSERTIONS_ENABLED

# Place nanoapps in a namespace.
COMMON_CFLAGS += -DCHRE_NANOAPP_INTERNAL

# Define CUST_H to allow including the customer header file.
HEXAGON_CFLAGS += -DCUST_H

# Makefile Includes ############################################################

# Variant-specific includes.
include $(CHRE_VARIANT_MK_INCLUDES)

# CHRE Implementation includes.
include apps/apps.mk
include ash/ash.mk
include chre_api/chre_api.mk
include core/core.mk
include external/external.mk
include pal/pal.mk
include platform/platform.mk
include util/util.mk

# Common includes.
include build/common.mk

# Supported Variants Includes. Not all CHRE variants are supported by this
# implementation of CHRE. Example: this CHRE implementation is never built for
# google_cm4_nanohub as Nanohub itself is a CHRE implementation.
include $(CHRE_PREFIX)/build/variant/google_hexagonv60_slpi.mk
include $(CHRE_PREFIX)/build/variant/google_hexagonv62_slpi.mk
include $(CHRE_PREFIX)/build/variant/google_x86_linux.mk
include $(CHRE_PREFIX)/build/variant/google_x86_googletest.mk
