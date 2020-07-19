-include $(TOPDIR)/rules.mk

CFLAGS := -fPIC -Wall -g -MMD
CPPFLAGS := $(CFLAGS)
LDFLAGS := -shared -fPIC

LIB := libwigig_pciaccess.so

.DEFAULT_GOAL = all

ifneq ($(CONFIG_TARGET_ipq)$(CONFIG_TARGET_ipq806x)$(CONFIG_TARGET_ipq807x)$(CONFIG_TARGET_ipq807x_64),)
    is_ipq = 1
endif

ifeq ($(WIGIG_3PP_BUILD), TRUE)
    #third-party build
    CROSS := $(TARGET_CROSS)
    CPPFLAGS := --sysroot=$(SYSROOT_CPP) $(CPPFLAGS)
    INCLUDE_CFLAGS += -I$(STAGING_DIR_PLATFORM)/usr/include/libnl3
    LDFLAGS += --sysroot=$(SYSROOT_CPP) -L$(STAGING_DIR_PLATFORM)/usr/lib
else ifeq ($(is_ipq), 1)
    ifneq ($(strip $(TOOLPREFIX)),)
        CROSS:=$(TOOLPREFIX)
    endif
endif

CC = $(CROSS)gcc
CXX = $(CROSS)g++

INCLUDES = \
    -I . \
    -I ../inc/linux \
    -I ../inc \
    -I ../utils/linux \
    -I ../utils \
    -I ./linux \

all: $(LIB)

CPP_FILES := $(shell find . -type f -name '*.cpp')
C_FILES := $(shell find . -type f -name '*.c')

OBJ_FILES :=  $(CPP_FILES:.cpp=.o)
OBJ_FILES +=  $(C_FILES:.c=.o)

$(LIB): $(OBJ_FILES)
	$(CXX) $(LDFLAGS) -o $(LIB) $(OBJ_FILES)

%.o : %.cpp
	$(CXX) $(CPPFLAGS) $(INCLUDES) -o $@ -c $<

%.o : %.c
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ -c $<

clean:
	rm -rf $(LIB)
	find . -type f \( -name "*.d" -o -name "*.o" \) -delete

-include $(OBJ_FILES:%.o=%.d)
