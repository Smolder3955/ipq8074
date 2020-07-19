LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := shell_11ad

LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true

LOCAL_CPPFLAGS := -lpthread -fexceptions
LOCAL_CFLAGS :=

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../External/JsonCpp/1.8.1/dist \
	$(LOCAL_PATH)/../shared \

LOCAL_SRC_FILES := $(shell find $(LOCAL_PATH) $(LOCAL_PATH)/../External/JsonCpp -type f \( -name "*.c" -o -name "*.cpp" \) | sed s:^$(LOCAL_PATH)::g)

include $(LOCAL_PATH)/../shared/BuildInfoAndroid.mk

include $(BUILD_EXECUTABLE)
