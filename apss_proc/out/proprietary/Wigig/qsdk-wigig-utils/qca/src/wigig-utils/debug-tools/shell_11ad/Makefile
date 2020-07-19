-include $(TOPDIR)/rules.mk

CPPFLAGS := -Wall -Wextra -g -MMD -std=c++0x -fPIE
CFLAGS := -Wall -Wextra -g -MMD -fPIE
LDFLAGS := -pthread -fPIE -pie

ifneq ($(CONFIG_TARGET_ipq)$(CONFIG_TARGET_ipq806x)$(CONFIG_TARGET_ipq807x)$(CONFIG_TARGET_ipq807x_64),)
    is_ipq = 1
endif

ifeq ($(WIGIG_3PP_BUILD), TRUE)
    #third-party build
    CROSS := $(TARGET_CROSS)
    CPPFLAGS := --sysroot=$(SYSROOT_CPP) $(CPPFLAGS)
    LDFLAGS += --sysroot=$(SYSROOT_CPP) -L$(STAGING_DIR_PLATFORM)/usr/lib
else ifeq ($(is_ipq), 1)
    ifneq ($(strip $(TOOLPREFIX)),)
        CROSS:=$(TOOLPREFIX)
    endif
    # OpenWrt
    PKG_CONFIG := pkg-config
endif

CC := $(CROSS)gcc
CXX := $(CROSS)g++

include ../shared/BuildInfoLinux.mk

.DEFAULT_GOAL = all
PROG = shell_11ad

INCLUDE_CFLAGS += \
	-I .\
	-I ../External/JsonCpp/1.8.1/dist \
	-I ../shared \

all: $(PROG)

C_FILES = $(shell find . ../External/JsonCpp -type f -name '*.c')
CPP_FILES = $(shell find . ../External/JsonCpp -type f -name '*.cpp')
OBJ_FILES =  $(CPP_FILES:.cpp=.o)
OBJ_FILES += $(C_FILES:.c=.o)

$(PROG): $(OBJ_FILES)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o : %.cpp
	$(CXX) $(CPPFLAGS) $(INCLUDE_CFLAGS) -o $@ -c $<

%.o : %.c
	$(CC) $(CFLAGS) $(INCLUDE_CFLAGS) -o $@ -c $<

clean:
	rm -rf $(PROG)
	find . ../External/JsonCpp -type f \( -name "*.d" -o -name "*.o" -o -name "*~" \) -delete

-include $(OBJ_FILES:%.o=%.d)
