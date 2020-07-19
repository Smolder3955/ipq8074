LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := cnss-daemon
ifeq ($(PRODUCT_VENDOR_MOVE_ENABLED),true)
LOCAL_PROPRIETARY_MODULE := true
endif
LOCAL_MODULE_TAGS := optional

LOCAL_CLANG := true
LOCAL_C_INCLUDES := \
	$(TARGET_OUT_HEADERS)/qmi-framework/inc \
	$(TARGET_OUT_HEADERS)/qmi/inc \
	$(TARGET_OUT_HEADERS)/qmi-framework/qcci/inc \
	$(TARGET_OUT_HEADERS)/external/libnl/include \
	$(TARGET_OUT_HEADERS)/cld80211-lib

ifeq ($(strip $(TARGET_USES_ICNSS_QMI)),true)
LOCAL_CFLAGS += -DICNSS_QMI

LOCAL_C_INCLUDES += \
	$(TARGET_OUT_HEADERS)/libmdmdetect/inc \
	$(TARGET_OUT_HEADERS)/libperipheralclient/inc
endif

ifneq ($(strip $(TARGET_USES_NO_SUBNET_DETECTION)),true)
LOCAL_CFLAGS += -DENABLE_SUBNET_DETECTION
endif

LOCAL_LD_FLAGS := -Wl,--hash-style=sysv

LOCAL_SRC_FILES := \
	main.c \
	nl_loop.c \
	debug.c \
	cnss_qmi_client.c \
	wireless_lan_proxy_service_v01.c \
	cnss_dp.c

ifeq ($(strip $(TARGET_USES_ICNSS_QMI)),true)
LOCAL_SRC_FILES += wlfw_qmi_client.c \
		wlan_firmware_service_v01.c
endif

ifeq ($(strip $(TARGET_USES_CNSS_CLI)),true)
LOCAL_SRC_FILES += \
	cnss_user.c \
	cnss_genl.c
endif

ifneq ($(strip $(TARGET_USES_NO_SUBNET_DETECTION)),true)
LOCAL_SRC_FILES += cnss_gw_update.c
endif

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	liblog \
	libqmi_cci \
	libqmi_common_so \
	libnl \
	libcld80211

LOCAL_HEADER_LIBRARIES += libril-qc-qmi-services-headers

ifeq ($(strip $(TARGET_USES_ICNSS_QMI)),true)
LOCAL_SHARED_LIBRARIES += \
	libperipheral_client \
	libmdmdetect \
	libqmiservices
endif

LOCAL_CFLAGS += \
	-DCONFIG_DEBUG \
	-DCONFIG_DEBUG_LOGCAT \
	-DCONFIG_CNSS_DP \
	-DCONFIG_CNSS_GW_UPDATE \
	-DCONFIG_CNSS_LPASS_SCAN \
	-DCONFIG_WLAN_MSG_SVC \
	-DCONFIG_READ_NV_MAC_ADDR

ifeq ($(strip $(TARGET_USES_CNSS_CLI)),true)
LOCAL_CFLAGS += -DCONFIG_CNSS_USER
endif

include $(BUILD_EXECUTABLE)

##################################################

ifeq ($(strip $(TARGET_USES_CNSS_CLI)),true)
include $(CLEAR_VARS)
LOCAL_MODULE := cnss_cli
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_TAGS := debug
LOCAL_SRC_FILES := cnss_cli.c
include $(BUILD_EXECUTABLE)
endif
