include $(CHRE_PREFIX)/build/clean_build_template_args.mk

TARGET_NAME = google_armv8a_aoc
ifneq ($(filter $(TARGET_NAME)% all, $(MAKECMDGOALS)),)

# Environment Checks
ifeq ($(AOC_TOP_DIR),)
$(error "The AoC root source directory needs to be exported as the AOC_TOP_DIR \
         environment variable")
endif

ifeq ($(AOC_PLATFORM) ,)
$(error "The platform/simulator we're building for needs to be exported as the \
         AOC_PLATFORM environment variable. For example: \
         export AOC_PLATFORM=qemu")
endif

SUPPORTED_AOC_PLATFORMS_ON_CHRE := fpga_a32 whi_a0_a32
ifeq ($(filter $(AOC_PLATFORM),$(SUPPORTED_AOC_PLATFORMS_ON_CHRE)),)
$(error "Unsupported AoC Platform - $(AOC_PLATFORM) - try PLATFORM=fpga_a32 or\
         whi_a0_a32")
endif

# The SRC_DIR variable is used in toolchain selection in AoC, add that as
# a dependency before including the platform toolchain makefile
ifeq ($(SRC_DIR),)
SRC_DIR=$(AOC_TOP_DIR)/AOC
endif

include $(AOC_TOP_DIR)/AOC/build/$(AOC_PLATFORM)/toolchain.mk
include $(AOC_TOP_DIR)/AOC/targets/aoc.$(AOC_PLATFORM)/local.mk

# Sized based on the buffer allocated in the host daemon (4096 bytes), minus
# FlatBuffer overhead (max 80 bytes), minus some extra space to make a nice
# round number and allow for addition of new fields to the FlatBuffer
TARGET_CFLAGS = -DCHRE_MESSAGE_TO_HOST_MAX_SIZE=4000
TARGET_CFLAGS += $(AOC_CFLAGS)
TARGET_CFLAGS += $(FREERTOS_CFLAGS)
TARGET_CFLAGS += -I$(AOC_TOP_DIR)/AOC/libs/common/basic/include
TARGET_CFLAGS += -I$(AOC_TOP_DIR)/external/libcxx/include

# libc / libm headers must be included after libcxx headers in case libcxx
# headers utilize #include_next
TARGET_CFLAGS += -I$(AOC_TOP_DIR)/AOC/libs/bionic_interface/include
TARGET_CFLAGS += -I$(AOC_TOP_DIR)/AOC/libs/common/libc/include

# Used to expose libc headers to nanoapps that aren't supported on the given platform
TARGET_CFLAGS += -I$(CHRE_PREFIX)/platform/shared/include/chre/platform/shared/libc

# add platform specific flags
ifeq ($(AOC_PLATFORM),linux)
TARGET_CFLAGS += $(AOC_LINUX_CFLAGS)
endif
ifeq ($(AOC_PLATFORM),fpga_a32)
TARGET_CFLAGS += $(AOC_FPGA_A32_CFLAGS)
# We need a function stack size of at least 400 bytes, which might not be
# the case by default
TARGET_CFLAGS += -Wframe-larger-than=420
endif
ifeq ($(AOC_PLATFORM),whi_a0_a32)
TARGET_CFLAGS += $(AOC_WHI_A0_A32_CFLAGS)
# We need a function stack size of at least 400 bytes, which might not be
# the case by default
TARGET_CFLAGS += -Wframe-larger-than=420
endif

TARGET_VARIANT_SRCS += $(AOC_SRCS)
TARGET_VARIANT_SRCS += $(FREERTOS_SRCS)
TARGET_VARIANT_SRCS += $(GOOGLE_AOC_SRCS)

TARGET_PLATFORM_ID = 0x476F6F676C000008

# Set platform-based build variables for arch armv8a
ARMV8A_AR = $(CLANG_PATH)/llvm-ar
ARMV8A_CC = $(CXX)
ARMV8A_LD = $(LD)

ifneq ($(IS_NANOAPP_BUILD),)
ifeq ($(CHRE_NANOAPP_BUILD_ID),)
# Set to "local" to indicate this was generated locally. Any production build
# of the nanoapp will specify this ID.
CHRE_NANOAPP_BUILD_ID=local
endif

TARGET_CFLAGS += -DNANOAPP_BUILD_ID=\"nanoapp_build=$(NANOAPP_NAME)@$(CHRE_NANOAPP_BUILD_ID)\"

include $(CHRE_PREFIX)/build/nanoapp/google_aoc.mk
ifeq ($(CHRE_TCM_ENABLED),true)
TARGET_CFLAGS += -DCHRE_TCM_ENABLED
# Flags:  Signed | TCM
TARGET_NANOAPP_FLAGS = 0x00000005
endif
endif

include $(CHRE_PREFIX)/build/arch/armv8a.mk
include $(CHRE_PREFIX)/build/build_template.mk
endif
