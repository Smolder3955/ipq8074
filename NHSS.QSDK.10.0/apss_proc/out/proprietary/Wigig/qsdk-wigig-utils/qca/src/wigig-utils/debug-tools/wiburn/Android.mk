LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := wigig_wiburn

LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true

LOCAL_CPPFLAGS := -Wall -lpthread -fexceptions

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/../lib/WlctPciAcss \
	$(LOCAL_PATH)/../lib/FlashAcss \
	$(LOCAL_PATH)/../lib/inc \
	$(LOCAL_PATH)/../lib/utils

LOCAL_SHARED_LIBRARIES := \
	libwigig_utils \
	libwigig_pciaccess \
	libwigig_flashaccess

LOCAL_SRC_FILES := \
	wiburn.cpp \
	translation_map.cpp \
	ParameterParser.cpp \
	template_inst.cpp \
	CCRC32.cpp

include $(BUILD_EXECUTABLE)

