#
# Platform Makefile
#

include $(CHRE_PREFIX)/external/flatbuffers/flatbuffers.mk

# Common Compiler Flags ########################################################

# Include paths.
COMMON_CFLAGS += -Iplatform/include

# SLPI-specific Compiler Flags #################################################

# Include paths.
SLPI_CFLAGS += -I$(SLPI_PREFIX)/build/ms
SLPI_CFLAGS += -I$(SLPI_PREFIX)/build/cust
SLPI_CFLAGS += -I$(SLPI_PREFIX)/core/api/debugtools
SLPI_CFLAGS += -I$(SLPI_PREFIX)/core/api/services
SLPI_CFLAGS += -I$(SLPI_PREFIX)/core/api/kernel/devcfg
SLPI_CFLAGS += -I$(SLPI_PREFIX)/core/api/kernel/qurt
SLPI_CFLAGS += -I$(SLPI_PREFIX)/core/api/dal
SLPI_CFLAGS += -I$(SLPI_PREFIX)/core/api/mproc
SLPI_CFLAGS += -I$(SLPI_PREFIX)/core/api/systemdrivers
SLPI_CFLAGS += -I$(SLPI_PREFIX)/platform/inc
SLPI_CFLAGS += -I$(SLPI_PREFIX)/platform/inc/HAP
SLPI_CFLAGS += -I$(SLPI_PREFIX)/platform/inc/a1std
SLPI_CFLAGS += -I$(SLPI_PREFIX)/platform/inc/stddef
SLPI_CFLAGS += -I$(SLPI_PREFIX)/platform/rtld/inc
SLPI_CFLAGS += -I$(SLPI_PREFIX)/ssc/goog/api
SLPI_CFLAGS += -I$(SLPI_PREFIX)/ssc/inc
SLPI_CFLAGS += -I$(SLPI_PREFIX)/ssc/inc/internal

SLPI_CFLAGS += -Iplatform/shared/include
SLPI_CFLAGS += -Iplatform/slpi/include

# We use FlatBuffers in the SLPI platform layer
SLPI_CFLAGS += $(FLATBUFFERS_CFLAGS)

# Needed to define __SIZEOF_ATTR_THREAD in sns_osa_thread.h, included in
# sns_memmgr.h.
SLPI_CFLAGS += -DSSC_TARGET_HEXAGON

ifneq ($(CHRE_ENABLE_ACCEL_CAL), false)
SLPI_CFLAGS += -DCHRE_ENABLE_ACCEL_CAL
endif

ifneq ($(CHRE_ENABLE_ASH_DEBUG_DUMP), false)
SLPI_CFLAGS += -DCHRE_ENABLE_ASH_DEBUG_DUMP
endif

# SLPI/SEE-specific Compiler Flags #############################################

# Include paths.
SLPI_SEE_CFLAGS += -I$(SLPI_PREFIX)/chre/chre/src/system/chre/platform/slpi
SLPI_SEE_CFLAGS += -I$(SLPI_PREFIX)/core/api/kernel/libstd/stringl
SLPI_SEE_CFLAGS += -I$(SLPI_PREFIX)/qmimsgs/common/api
SLPI_SEE_CFLAGS += -I$(SLPI_PREFIX)/ssc_api/pb
SLPI_SEE_CFLAGS += -I$(SLPI_PREFIX)/ssc/framework/cm/inc
SLPI_SEE_CFLAGS += -I$(SLPI_PREFIX)/ssc/inc/utils/nanopb

SLPI_SEE_CFLAGS += -Iplatform/slpi/see/include

SLPI_SEE_CFLAGS += -DCHRE_SLPI_SEE

# Defined in slpi_proc/ssc/build/ssc.scons
SLPI_SEE_CFLAGS += -DPB_FIELD_16BIT

ifeq ($(IMPORT_CHRE_UTILS), true)
SLPI_SEE_CFLAGS += -DIMPORT_CHRE_UTILS
endif

# SLPI-specific Source Files ###################################################

SLPI_SRCS += platform/shared/assert.cc
SLPI_SRCS += platform/shared/chre_api_audio.cc
SLPI_SRCS += platform/shared/chre_api_core.cc
SLPI_SRCS += platform/shared/chre_api_gnss.cc
SLPI_SRCS += platform/shared/chre_api_re.cc
SLPI_SRCS += platform/shared/chre_api_sensor.cc
SLPI_SRCS += platform/shared/chre_api_version.cc
SLPI_SRCS += platform/shared/chre_api_wifi.cc
SLPI_SRCS += platform/shared/chre_api_wwan.cc
SLPI_SRCS += platform/shared/host_protocol_chre.cc
SLPI_SRCS += platform/shared/host_protocol_common.cc
SLPI_SRCS += platform/shared/memory_manager.cc
SLPI_SRCS += platform/shared/nanoapp_load_manager.cc
SLPI_SRCS += platform/shared/nanoapp/nanoapp_dso_util.cc
SLPI_SRCS += platform/shared/pal_system_api.cc
SLPI_SRCS += platform/shared/pw_tokenized_log.cc
SLPI_SRCS += platform/shared/system_time.cc
SLPI_SRCS += platform/slpi/chre_api_re.cc
SLPI_SRCS += platform/slpi/fatal_error.cc
SLPI_SRCS += platform/slpi/host_link.cc
SLPI_SRCS += platform/slpi/init.cc
SLPI_SRCS += platform/slpi/memory.cc
SLPI_SRCS += platform/slpi/memory_manager.cc
SLPI_SRCS += platform/slpi/platform_debug_dump_manager.cc
SLPI_SRCS += platform/slpi/platform_nanoapp.cc
SLPI_SRCS += platform/slpi/platform_pal.cc
SLPI_SRCS += platform/slpi/platform_sensor_type_helpers.cc
SLPI_SRCS += platform/slpi/system_time.cc
SLPI_SRCS += platform/slpi/system_time_util.cc
SLPI_SRCS += platform/slpi/system_timer.cc

# Optional audio support.
ifeq ($(CHRE_AUDIO_SUPPORT_ENABLED), true)
SLPI_SRCS += platform/slpi/platform_audio.cc
endif

# Optional GNSS support.
ifeq ($(CHRE_GNSS_SUPPORT_ENABLED), true)
SLPI_SRCS += platform/shared/platform_gnss.cc
endif

# Optional Wi-Fi support.
ifeq ($(CHRE_WIFI_SUPPORT_ENABLED), true)
SLPI_SRCS += platform/shared/platform_wifi.cc
endif

# Optional WWAN support.
ifeq ($(CHRE_WWAN_SUPPORT_ENABLED), true)
SLPI_SRCS += platform/shared/platform_wwan.cc
endif

# SLPI/SEE-specific Source Files ###############################################

# Optional sensors support.
ifeq ($(CHRE_SENSORS_SUPPORT_ENABLED), true)
SLPI_SEE_SRCS += platform/slpi/see/platform_sensor.cc
SLPI_SEE_SRCS += platform/slpi/see/platform_sensor_manager.cc
ifneq ($(IMPORT_CHRE_UTILS), true)
SLPI_SEE_SRCS += platform/slpi/see/see_cal_helper.cc
SLPI_SEE_SRCS += platform/slpi/see/see_helper.cc
endif

SLPI_SEE_SRCS += $(SLPI_PREFIX)/ssc_api/pb/sns_client.pb.c
SLPI_SEE_SRCS += $(SLPI_PREFIX)/ssc_api/pb/sns_suid.pb.c
SLPI_SEE_SRCS += $(SLPI_PREFIX)/ssc_api/pb/sns_cal.pb.c
SLPI_SEE_SRCS += $(SLPI_PREFIX)/ssc_api/pb/sns_physical_sensor_test.pb.c
SLPI_SEE_SRCS += $(SLPI_PREFIX)/ssc_api/pb/sns_proximity.pb.c
SLPI_SEE_SRCS += $(SLPI_PREFIX)/ssc_api/pb/sns_remote_proc_state.pb.c
SLPI_SEE_SRCS += $(SLPI_PREFIX)/ssc_api/pb/sns_resampler.pb.c
SLPI_SEE_SRCS += $(SLPI_PREFIX)/ssc_api/pb/sns_std.pb.c
SLPI_SEE_SRCS += $(SLPI_PREFIX)/ssc_api/pb/sns_std_sensor.pb.c
SLPI_SEE_SRCS += $(SLPI_PREFIX)/ssc_api/pb/sns_std_type.pb.c

SLPI_SEE_QSK_SRCS += $(SLPI_PREFIX)/chre/chre/src/system/chre/platform/slpi/sns_qmi_client_alt.c
SLPI_SEE_QMI_SRCS += $(SLPI_PREFIX)/chre/chre/src/system/chre/platform/slpi/sns_qmi_client.c
endif

SLPI_SEE_SRCS += platform/slpi/see/power_control_manager.cc

ifneq ($(IMPORT_CHRE_UTILS), true)
SLPI_SEE_SRCS += platform/slpi/see/island_vote_client.cc
endif

# FreeRTOS-specific Source Files ###############################################

FREERTOS_SRCS += platform/freertos/context.cc
FREERTOS_SRCS += platform/freertos/init.cc
FREERTOS_SRCS += platform/freertos/memory_manager.cc
FREERTOS_SRCS += platform/freertos/platform_debug_dump_manager.cc
FREERTOS_SRCS += platform/freertos/platform_nanoapp.cc

# Optional FreeRTOS-specific Source Files ######################################

# memory.cc is only needed if the given platform doesn't have its own memory
# allocation setup.
FREERTOS_OPTIONAL_SRCS += platform/freertos/memory.cc

# FreeRTOS-specific Compiler Flags #############################################
FREERTOS_CFLAGS += -I$(AOC_TOP_DIR)/external/FreeRTOS/FreeRTOS/Source/include
FREERTOS_CFLAGS += -Iplatform/freertos/include
FREERTOS_CFLAGS += -Iplatform/shared/include

# AoC-specific Source Files ####################################################

AOC_SRCS += chpp/transport.c
AOC_SRCS += chpp/app.c
AOC_SRCS += chpp/services.c
AOC_SRCS += chpp/services/discovery.c
AOC_SRCS += chpp/services/loopback.c
AOC_SRCS += chpp/services/nonhandle.c
AOC_SRCS += chpp/clients.c
AOC_SRCS += chpp/clients/discovery.c
AOC_SRCS += chpp/clients/loopback.c
AOC_SRCS += chpp/platform/pal_api.c
AOC_SRCS += chpp/platform/aoc/condition_variable.cc
AOC_SRCS += chpp/platform/aoc/link.cc
AOC_SRCS += chpp/platform/aoc/chpp_init.cc
AOC_SRCS += chpp/platform/aoc/chpp_task_util.cc
AOC_SRCS += chpp/platform/aoc/chpp_uart_link_manager.cc
AOC_SRCS += chpp/platform/aoc/memory.cc
AOC_SRCS += chpp/platform/aoc/notifier.cc
AOC_SRCS += chpp/platform/aoc/time.cc
AOC_SRCS += platform/aoc/audio_controller.cc
AOC_SRCS += platform/aoc/audio_filter.cc
AOC_SRCS += platform/aoc/chre_api_re.cc
AOC_SRCS += platform/aoc/dram_vote_client.cc
AOC_SRCS += platform/aoc/fatal_error.cc
AOC_SRCS += platform/aoc/host_link.cc
AOC_SRCS += platform/aoc/log.cc
AOC_SRCS += platform/aoc/memory.cc
AOC_SRCS += platform/aoc/power_control_manager.cc
AOC_SRCS += platform/aoc/platform_audio.cc
AOC_SRCS += platform/aoc/platform_cache_management.cc
AOC_SRCS += platform/aoc/platform_pal.cc
AOC_SRCS += platform/aoc/system_time.cc
AOC_SRCS += platform/aoc/system_timer.cc
AOC_SRCS += platform/shared/assert.cc
AOC_SRCS += platform/shared/chre_api_audio.cc
AOC_SRCS += platform/shared/chre_api_core.cc
AOC_SRCS += platform/shared/chre_api_gnss.cc
AOC_SRCS += platform/shared/chre_api_re.cc
AOC_SRCS += platform/shared/chre_api_sensor.cc
AOC_SRCS += platform/shared/chre_api_version.cc
AOC_SRCS += platform/shared/chre_api_wifi.cc
AOC_SRCS += platform/shared/chre_api_wwan.cc
AOC_SRCS += platform/shared/dlfcn.cc
AOC_SRCS += platform/shared/dram_vote_client.cc
AOC_SRCS += platform/shared/host_protocol_chre.cc
AOC_SRCS += platform/shared/host_protocol_common.cc
AOC_SRCS += platform/shared/memory_manager.cc
AOC_SRCS += platform/shared/nanoapp_loader.cc
AOC_SRCS += platform/shared/nanoapp_load_manager.cc
AOC_SRCS += platform/shared/pal_system_api.cc
AOC_SRCS += platform/shared/pal_sensor_stub.cc
AOC_SRCS += platform/shared/system_time.cc
AOC_SRCS += platform/shared/nanoapp/nanoapp_dso_util.cc
AOC_SRCS += platform/usf/platform_sensor.cc
AOC_SRCS += platform/usf/platform_sensor_manager.cc
AOC_SRCS += platform/usf/platform_sensor_type_helpers.cc
AOC_SRCS += platform/usf/usf_helper.cc

# Optional GNSS support.
ifeq ($(CHRE_GNSS_SUPPORT_ENABLED), true)
AOC_SRCS += platform/shared/platform_gnss.cc
AOC_SRCS += chpp/clients/gnss.c
AOC_CFLAGS += -DCHPP_CLIENT_ENABLED_CHRE_GNSS
endif

# Optional Wi-Fi support.
ifeq ($(CHRE_WIFI_SUPPORT_ENABLED), true)
AOC_SRCS += platform/shared/platform_wifi.cc
AOC_SRCS += chpp/clients/wifi.c
AOC_CFLAGS += -DCHPP_CLIENT_ENABLED_CHRE_WIFI
endif

# Optional WWAN support.
ifeq ($(CHRE_WWAN_SUPPORT_ENABLED), true)
AOC_SRCS += platform/shared/platform_wwan.cc
AOC_SRCS += chpp/clients/wwan.c
AOC_CFLAGS += -DCHPP_CLIENT_ENABLED_CHRE_WWAN
endif

# AoC-specific Compiler Flags ##################################################
AOC_CFLAGS += -Iplatform/aoc/include
AOC_CFLAGS += -Iplatform/usf/include
AOC_CFLAGS += -I$(AOC_AUTOGEN_DIR)/regions
AOC_CFLAGS += -I$(AOC_TOP_DIR)/AOC
AOC_CFLAGS += -I$(AOC_TOP_DIR)/AOC/apps
AOC_CFLAGS += -I$(AOC_TOP_DIR)/AOC/core/arm/generic/include
AOC_CFLAGS += -I$(AOC_TOP_DIR)/AOC/drivers/gpio/aoc/include
AOC_CFLAGS += -I$(AOC_TOP_DIR)/AOC/drivers/gpio/base/include
AOC_CFLAGS += -I$(AOC_TOP_DIR)/AOC/drivers/gpi/aoc/include
AOC_CFLAGS += -I$(AOC_TOP_DIR)/AOC/drivers/gpi/base/include
AOC_CFLAGS += -I$(AOC_TOP_DIR)/AOC/drivers/processor
AOC_CFLAGS += -I$(AOC_TOP_DIR)/AOC/drivers/processor/aoc/include
AOC_CFLAGS += -I$(AOC_TOP_DIR)/AOC/drivers/sys_mem/base/include
AOC_CFLAGS += -I$(AOC_TOP_DIR)/AOC/efw/include
AOC_CFLAGS += -I$(AOC_TOP_DIR)/AOC/filters/audio/common/include
AOC_CFLAGS += -I$(AOC_TOP_DIR)/AOC/libs/common/heap/common/include
AOC_CFLAGS += -I$(AOC_TOP_DIR)/AOC/os/common/include
AOC_CFLAGS += -I$(AOC_TOP_DIR)/AOC/platform/common/include
AOC_CFLAGS += -I$(AOC_TOP_DIR)/AOC/product/$(AOC_PRODUCT_FAMILY)/include
AOC_CFLAGS += -I$(AOC_TOP_DIR)/usf/core/include
AOC_CFLAGS += -I$(AOC_TOP_DIR)/usf/core/fbs
AOC_CFLAGS += -I$(AOC_TOP_DIR)/usf/pal/include
AOC_CFLAGS += -I$(AOC_TOP_DIR)/usf/pal/efw/include

AOC_CFLAGS += -Ichpp/include
AOC_CFLAGS += -Ichpp/platform/aoc/include
AOC_CFLAGS += -DCHPP_CLIENT_ENABLED_DISCOVERY

# We use FlatBuffers in the AOC platform layer
AOC_CFLAGS += $(FLATBUFFERS_CFLAGS)

# Ensure USF uses its own flatbuffers header. This is needed while USF migrates
# away from CHRE's header.
AOC_CFLAGS += -DFLATBUFFERS_USF

# Simulator-specific Compiler Flags ############################################

SIM_CFLAGS += -Iplatform/shared/include

# Simulator-specific Source Files ##############################################

SIM_SRCS += platform/linux/chre_api_re.cc
SIM_SRCS += platform/linux/context.cc
SIM_SRCS += platform/linux/fatal_error.cc
SIM_SRCS += platform/linux/host_link.cc
SIM_SRCS += platform/linux/memory.cc
SIM_SRCS += platform/linux/memory_manager.cc
SIM_SRCS += platform/linux/platform_debug_dump_manager.cc
SIM_SRCS += platform/linux/platform_log.cc
SIM_SRCS += platform/linux/platform_pal.cc
SIM_SRCS += platform/linux/platform_sensor_type_helpers.cc
SIM_SRCS += platform/linux/power_control_manager.cc
SIM_SRCS += platform/linux/system_time.cc
SIM_SRCS += platform/linux/system_timer.cc
SIM_SRCS += platform/linux/platform_nanoapp.cc
SIM_SRCS += platform/linux/platform_sensor.cc
SIM_SRCS += platform/linux/platform_sensor_type_helpers.cc
SIM_SRCS += platform/shared/chre_api_audio.cc
SIM_SRCS += platform/shared/chre_api_core.cc
SIM_SRCS += platform/shared/chre_api_gnss.cc
SIM_SRCS += platform/shared/chre_api_re.cc
SIM_SRCS += platform/shared/chre_api_sensor.cc
SIM_SRCS += platform/shared/chre_api_version.cc
SIM_SRCS += platform/shared/chre_api_wifi.cc
SIM_SRCS += platform/shared/chre_api_wwan.cc
SIM_SRCS += platform/shared/memory_manager.cc
SIM_SRCS += platform/shared/nanoapp/nanoapp_dso_util.cc
SIM_SRCS += platform/shared/pal_sensor_stub.cc
SIM_SRCS += platform/shared/pal_system_api.cc
SIM_SRCS += platform/shared/platform_sensor_manager.cc
SIM_SRCS += platform/shared/system_time.cc

# Optional GNSS support.
ifeq ($(CHRE_GNSS_SUPPORT_ENABLED), true)
SIM_SRCS += platform/linux/pal_gnss.cc
SIM_SRCS += platform/shared/platform_gnss.cc
endif

# Optional Wi-Fi support.
ifeq ($(CHRE_WIFI_SUPPORT_ENABLED), true)
SIM_SRCS += platform/linux/pal_wifi.cc
SIM_SRCS += platform/shared/platform_wifi.cc
endif

# Optional WWAN support.
ifeq ($(CHRE_WWAN_SUPPORT_ENABLED), true)
SIM_SRCS += platform/linux/pal_wwan.cc
SIM_SRCS += platform/shared/platform_wwan.cc
endif

# Linux-specific Compiler Flags ################################################

GOOGLE_X86_LINUX_CFLAGS += -Iplatform/linux/include

# Linux-specific Source Files ##################################################

GOOGLE_X86_LINUX_SRCS += platform/linux/init.cc
GOOGLE_X86_LINUX_SRCS += platform/linux/assert.cc

# Optional audio support.
ifeq ($(CHRE_AUDIO_SUPPORT_ENABLED), true)
GOOGLE_X86_LINUX_SRCS += platform/linux/audio_source.cc
GOOGLE_X86_LINUX_SRCS += platform/linux/platform_audio.cc
endif

# Android-specific Compiler Flags ##############################################

# Add the Android include search path for Android-specific header files.
GOOGLE_ARM64_ANDROID_CFLAGS += -Iplatform/android/include

# Add in host sources to allow the executable to both be a socket server and
# CHRE implementation.
GOOGLE_ARM64_ANDROID_CFLAGS += -I$(ANDROID_BUILD_TOP)/system/core/base/include
GOOGLE_ARM64_ANDROID_CFLAGS += -I$(ANDROID_BUILD_TOP)/system/core/libcutils/include
GOOGLE_ARM64_ANDROID_CFLAGS += -I$(ANDROID_BUILD_TOP)/system/core/libutils/include
GOOGLE_ARM64_ANDROID_CFLAGS += -I$(ANDROID_BUILD_TOP)/system/core/liblog/include
GOOGLE_ARM64_ANDROID_CFLAGS += -Ihost/common/include

# Also add the linux sources to fall back to the default Linux implementation.
GOOGLE_ARM64_ANDROID_CFLAGS += -Iplatform/linux/include

# We use FlatBuffers in the Android simulator
GOOGLE_ARM64_ANDROID_CFLAGS += -I$(FLATBUFFERS_PATH)/include

# Android-specific Source Files ################################################

ANDROID_CUTILS_TOP = $(ANDROID_BUILD_TOP)/system/core/libcutils
ANDROID_LOG_TOP = $(ANDROID_BUILD_TOP)/system/core/liblog

GOOGLE_ARM64_ANDROID_SRCS += $(ANDROID_CUTILS_TOP)/sockets_unix.cpp
GOOGLE_ARM64_ANDROID_SRCS += $(ANDROID_CUTILS_TOP)/android_get_control_file.cpp
GOOGLE_ARM64_ANDROID_SRCS += $(ANDROID_CUTILS_TOP)/socket_local_server_unix.cpp
GOOGLE_ARM64_ANDROID_SRCS += $(ANDROID_CUTILS_TOP)/socket_local_client_unix.cpp
GOOGLE_ARM64_ANDROID_SRCS += $(ANDROID_LOG_TOP)/logd_reader.c

GOOGLE_ARM64_ANDROID_SRCS += platform/android/init.cc
GOOGLE_ARM64_ANDROID_SRCS += platform/android/host_link.cc
GOOGLE_ARM64_ANDROID_SRCS += platform/shared/host_protocol_common.cc
GOOGLE_ARM64_ANDROID_SRCS += host/common/host_protocol_host.cc
GOOGLE_ARM64_ANDROID_SRCS += host/common/socket_server.cc

# Optional audio support.
ifeq ($(CHRE_AUDIO_SUPPORT_ENABLED), true)
GOOGLE_ARM64_ANDROID_SRCS += platform/android/platform_audio.cc
endif

# GoogleTest Compiler Flags ####################################################

# The order here is important so that the googletest target prefers shared,
# linux and then SLPI.
GOOGLETEST_CFLAGS += -Iplatform/shared/include
GOOGLETEST_CFLAGS += -Iplatform/linux/include
GOOGLETEST_CFLAGS += -Iplatform/slpi/include

# GoogleTest Source Files ######################################################

GOOGLETEST_COMMON_SRCS += platform/linux/assert.cc
GOOGLETEST_COMMON_SRCS += platform/linux/audio_source.cc
GOOGLETEST_COMMON_SRCS += platform/linux/platform_audio.cc
