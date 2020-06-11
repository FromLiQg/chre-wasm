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

# TODO(stange): Move most of these includes to platform.mk so nanoapps can't
# refer to AoC APIs
TARGET_CFLAGS += -I$(AOC_TOP_DIR)/AOC/core/arm/generic/include
TARGET_CFLAGS += -I$(AOC_TOP_DIR)/AOC/drivers/processor/aoc/include
TARGET_CFLAGS += -I$(AOC_TOP_DIR)/AOC/drivers/sys_mem/base/include
TARGET_CFLAGS += -I$(AOC_TOP_DIR)/AOC/libs/bionic_interface/include
TARGET_CFLAGS += -I$(AOC_TOP_DIR)/AOC/libs/common/basic/include
TARGET_CFLAGS += -I$(AOC_TOP_DIR)/AOC/libs/common/libc/include
TARGET_CFLAGS += -I$(AOC_TOP_DIR)/AOC/libs/common/heap/common/include
TARGET_CFLAGS += -I$(AOC_TOP_DIR)/external/libcxx/include

# TODO: Fix ar_client so the following two can be removed.
TARGET_CFLAGS += -Wno-strict-prototypes
TARGET_CFLAGS += -Wno-missing-prototypes

# TODO: Fix log_null so this can be removed.
TARGET_CFLAGS += -Wno-unused-parameter

TARGET_CFLAGS += -Wframe-larger-than=1024

# Don't add these flags when building just the static archive as they generate
# sections that will break static linking.
ifeq ($(IS_ARCHIVE_ONLY_BUILD),)
TARGET_CFLAGS += -fPIC
TARGET_SO_LDFLAGS += -shared
endif
