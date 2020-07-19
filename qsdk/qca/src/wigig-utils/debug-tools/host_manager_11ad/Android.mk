LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := host_manager_11ad
LOCAL_MODULE_TAGS := optional
LOCAL_PROPRIETARY_MODULE := true

LOCAL_CPPFLAGS := -lpthread -fexceptions
LOCAL_CFLAGS :=

# in Android lib-genl is part of libnl
LOCAL_SHARED_LIBRARIES := libnl

LOCAL_C_INCLUDES += \
	$(LOCAL_PATH)/access_layer_11ad \
	$(LOCAL_PATH)/access_layer_11ad/Unix \
	$(LOCAL_PATH)/../External/JsonCpp/1.8.1/dist \
	$(LOCAL_PATH)/../External/SimpleWebServer/3.0.0 \
	$(LOCAL_PATH)/../External/Asio/1.10.6/include \
	$(LOCAL_PATH)/../shared \
	external/libnl/include \

# For host_manager_11ad SPI build run:
# mm WIGIG_ARCH_SPI=TRUE | tee buildlog.txt
$(info ***** $$WIGIG_ARCH_SPI is [${WIGIG_ARCH_SPI}] *****)
ifeq ($(WIGIG_ARCH_SPI), TRUE)
$(info ***** Building host_manager for SPI architecture *****)
LOCAL_CPPFLAGS += -D_WIGIG_ARCH_SPI
LOCAL_C_INCLUDES +=                             \
        $(LOCAL_PATH)/access_layer_11ad/SPI     \
        $(TARGET_OUT_HEADERS)/sensors/inc       \
        $(TARGET_OUT_HEADERS)/common/inc        \
        $(TARGET_OUT_HEADERS)/qmi/inc

LOCAL_SHARED_LIBRARIES += libsensor1
endif

# SLPI Sensing support
ifneq ($(filter kona,$(TARGET_BOARD_PLATFORM)),)
$(info ***** Building host_manager with SLPI sensing support *****)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_CLANG := true

LOCAL_CPPFLAGS += -D_WIGIG_SLPI_SENSING_
LOCAL_C_INCLUDES += $(TARGET_OUT_HEADERS)/libwigigsensing
LOCAL_SHARED_LIBRARIES += libwigigsensing
endif

LOCAL_SRC_FILES := $(shell find $(LOCAL_PATH) $(LOCAL_PATH)/../External -type f \( -name "*.c" -o -name "*.cpp" \) | sed s:^$(LOCAL_PATH)::g)

include $(LOCAL_PATH)/../shared/BuildInfoAndroid.mk

include $(BUILD_EXECUTABLE)
