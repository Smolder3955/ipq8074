-include $(TOPDIR)/rules.mk

CPPFLAGS := -Wall -Wextra -g -MMD -std=c++0x -fPIE
CFLAGS := -Wall -Wextra -g -MMD -fPIE
LDFLAGS := -pthread -fPIE -pie
LIBS := -lnl-3 -lnl-genl-3

ifneq ($(CONFIG_TARGET_ipq)$(CONFIG_TARGET_ipq806x)$(CONFIG_TARGET_ipq807x)$(CONFIG_TARGET_ipq807x_64),)
    is_ipq = 1
endif

ifeq ($(WIGIG_3PP_BUILD), TRUE)
    #third-party build
    CROSS := $(TARGET_CROSS)
    CFLAGS := --sysroot=$(SYSROOT_CPP) $(CFLAGS)
    CPPFLAGS := --sysroot=$(SYSROOT_CPP) $(CPPFLAGS)
    INCLUDE_CFLAGS += -I$(STAGING_DIR_PLATFORM)/usr/include/libnl3
    LDFLAGS += --sysroot=$(SYSROOT_CPP) -L$(STAGING_DIR_PLATFORM)/usr/lib
else ifeq ($(is_ipq), 1)
    ifneq ($(strip $(TOOLPREFIX)),)
        CROSS:=$(TOOLPREFIX)
    endif
    # OpenWrt
    PKG_CONFIG := pkg-config
    INCLUDE_CFLAGS += $(shell $(PKG_CONFIG) --cflags libnl-3.0)
    INCLUDE_CFLAGS += $(shell $(PKG_CONFIG) --cflags libnl-genl-3.0)
else
    ifneq (,$(findstring SUSE,$(shell lsb_release -a)))
        # RTL Simulation
        -include SUSE/SUSE.mk
    else
        # Ubuntu
        INCLUDE_CFLAGS += -I /usr/include/libnl3
    endif
endif

CC := $(CROSS)gcc
CXX := $(CROSS)g++

include ../shared/BuildInfoLinux.mk

.DEFAULT_GOAL = all
PROG = host_manager_11ad

INCLUDE_CFLAGS += \
	-I .\
	-I access_layer_11ad \
	-I access_layer_11ad/Unix \
	-I ../External/JsonCpp/1.8.1/dist \
	-I ../External/Asio/1.10.6/include \
	-I ../External/SimpleWebServer/3.0.0 \
	-I ../shared \

# SLPI Sensing test
#CPPFLAGS += -D_WIGIG_SLPI_SENSING_

all: $(PROG)

C_FILES = $(shell find . ../External -type f -name '*.c')
CPP_FILES = $(shell find . ../External -type f -name '*.cpp')
OBJ_FILES =  $(CPP_FILES:.cpp=.o)
OBJ_FILES += $(C_FILES:.c=.o)

$(PROG): $(OBJ_FILES)
	$(CXX) -o $@ $^ $(LDFLAGS) $(LIBS)

%.o : %.cpp
	$(CXX) $(CPPFLAGS) $(INCLUDE_CFLAGS) -o $@ -c $<

%.o : %.c
	$(CC) $(CFLAGS) $(INCLUDE_CFLAGS) -o $@ -c $<

clean:
	rm -rf $(PROG)
	find . ../External -type f \( -name "*.d" -o -name "*.o" -o -name "*~" \) -delete

-include $(OBJ_FILES:%.o=%.d)
