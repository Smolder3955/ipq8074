LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libwigig_utils
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true

LOCAL_CPPFLAGS := -Wall -lpthread

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/../inc \
	$(LOCAL_PATH)/../inc/linux \
	$(LOCAL_PATH)/linux

LOCAL_SRC_FILES := $(shell find $(LOCAL_PATH) -name '*.cpp' | sed s:^$(LOCAL_PATH)::g )

include $(BUILD_SHARED_LIBRARY)
