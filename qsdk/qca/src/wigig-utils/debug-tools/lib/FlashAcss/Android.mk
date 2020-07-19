LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := libwigig_flashaccess

LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true

LOCAL_CPPFLAGS := -Wall -lpthread

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH) \
	$(LOCAL_PATH)/../inc \
	$(LOCAL_PATH)/../inc/linux \
	$(LOCAL_PATH)/../utils \
	$(LOCAL_PATH)/../utils/linux \
	$(LOCAL_PATH)/linux \
	$(LOCAL_PATH)/../WlctPciAcss \
	$(LOCAL_PATH)/../WlctPciAcss/linux

LOCAL_SRC_FILES := $(shell find $(LOCAL_PATH) -name '*.cpp' | sed s:^$(LOCAL_PATH)::g )

LOCAL_SHARED_LIBRARIES := \
	libwigig_utils \
	libwigig_pciaccess

include $(BUILD_SHARED_LIBRARY)
