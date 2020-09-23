#
# Build targets for an ARMv8 based processor
#

# The ARMV8A_XX binary path needs to be specified by a
# higher level makefile
TARGET_AR = $(ARMV8A_AR)
TARGET_CC = $(ARMV8A_CC)
TARGET_LD = $(ARMV8A_LD)

TARGET_CFLAGS += $(DEFINES)
TARGET_CFLAGS += $(CPPFLAGS)
COMMON_CXX_CFLAGS += $(CXXFLAGS)
COMMON_C_CFLAGS += $(CFLAGS)

TARGET_SO_LDFLAGS += $(LDFLAGS)
TARGET_SO_EARLY_LIBS += $(LDLIBS)

# TODO: Fix ar_client so the following two can be removed.
TARGET_CFLAGS += -Wno-strict-prototypes
TARGET_CFLAGS += -Wno-missing-prototypes

# TODO: Fix log_null so this can be removed.
TARGET_CFLAGS += -Wno-unused-parameter

# CHRE's stack size is 8KB so leave a little room to ensure the stack isn't
# blown when calling nanoapps since nanoappStart will be invoked with some data
# on the stack.
TARGET_CFLAGS += -Wframe-larger-than=7000

# Don't add these flags when building just the static archive as they generate
# sections that will break static linking.
ifeq ($(IS_ARCHIVE_ONLY_BUILD),)
TARGET_CFLAGS += -fPIC
TARGET_SO_LDFLAGS += -shared

# Only generate debug symbols for debug dynamic libraries
TARGET_CFLAGS += -g0
TARGET_DEBUG_CFLAGS += -g
endif
