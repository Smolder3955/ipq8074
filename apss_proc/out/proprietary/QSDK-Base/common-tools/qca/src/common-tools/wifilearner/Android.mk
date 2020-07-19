LOCAL_PATH := $(call my-dir)

HIDL_INTERFACE_VERSION = 1.0
INCLUDES = $(LOCAL_PATH)
INCLUDES += $(LOCAL_PATH)/hidl/$(HIDL_INTERFACE_VERSION)
INCLUDES += external/libnl/include

########################
include $(CLEAR_VARS)
LOCAL_MODULE := wifilearner
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true
LOCAL_CPPFLAGS := -Wall -Werror
LOCAL_CFLAGS := -Wall -Werror -Wno-unused-parameter
LOCAL_C_INCLUDES := $(INCLUDES)
LOCAL_SRC_FILES := \
    main.c \
    utils.c \
    nl_utils.c \
    hidl/$(HIDL_INTERFACE_VERSION)/hidl.cpp \
    hidl/$(HIDL_INTERFACE_VERSION)/wifistats.cpp
LOCAL_SHARED_LIBRARIES := \
    vendor.qti.hardware.wifi.wifilearner@1.0 \
    libbase \
    libhidlbase \
    libhidltransport \
    libhwbinder \
    libutils \
    liblog \
    libnl \
    libc \

include $(BUILD_EXECUTABLE)

