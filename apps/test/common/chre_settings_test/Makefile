#
# CHRE Settings Test Nanoapp Makefile
#

# Environment Checks ###########################################################
ifeq ($(CHRE_PREFIX),)
  ifneq ($(ANDROID_BUILD_TOP),)
    CHRE_PREFIX = $(ANDROID_BUILD_TOP)/system/chre
  else
    $(error "You must run 'lunch' to setup ANDROID_BUILD_TOP, or explicitly \
    define the CHRE_PREFIX environment variable to point to the CHRE root \
    directory.")
  endif
endif

# Nanoapp Configuration ########################################################

NANOAPP_NAME = chre_settings_test
NANOAPP_ID = 0x476f6f6754000003
NANOAPP_NAME_STRING = \"Chre\ Settings\ Test\"
NANOAPP_VERSION = 0x00000001

NANOAPP_PATH = $(CHRE_PREFIX)/apps/test/common/chre_settings_test

# Protobuf Sources #############################################################

NANOPB_EXTENSION = nanopb

NANOPB_SRCS += $(NANOAPP_PATH)/../proto/chre_settings_test.proto
NANOPB_INCLUDES = $(NANOAPP_PATH)/../proto

# Source Code ##################################################################

COMMON_SRCS += $(NANOAPP_PATH)/src/chre_settings_test.cc
COMMON_SRCS += $(NANOAPP_PATH)/src/chre_settings_test_manager.cc
COMMON_SRCS += $(NANOAPP_PATH)/src/chre_settings_test_util.cc
COMMON_SRCS += $(CHRE_PREFIX)/util/nanoapp/callbacks.cc

# Compiler Flags ###############################################################

# Defines
COMMON_CFLAGS += -DNANOAPP_MINIMUM_LOG_LEVEL=CHRE_LOG_LEVEL_INFO

# Includes
COMMON_CFLAGS += -I$(NANOAPP_PATH)/inc

# Permission declarations ######################################################

CHRE_NANOAPP_USES_GNSS = true
CHRE_NANOAPP_USES_WIFI = true
CHRE_NANOAPP_USES_WWAN = true
CHRE_NANOAPP_USES_AUDIO = true

# Makefile Includes ############################################################

include $(CHRE_PREFIX)/build/nanoapp/app.mk
