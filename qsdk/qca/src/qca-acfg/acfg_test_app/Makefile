CC=$(TOOLPREFIX)gcc

ACFG_BINARY ?= acfg_tool

SRCS=$(shell ls *.c)
OBJS=$(patsubst %.c,%.o,$(SRCS))
PWD=$(shell pwd)

CFLAGS := -I$(PWD)
CFLAGS += -I$(PWD)/../include

ifndef QSDK_ACFG
CFLAGS += -I$(PWD)/../../../drivers/wlan_modules/include/
CFLAGS += -I$(PWD)/../../../drivers/wlan_modules/cmn_dev/qdf/inc/
CFLAGS += -I$(PWD)/../../../drivers/wlan_modules/cmn_dev/qdf/linux/src/
CFLAGS += -I$(PWD)/../../../drivers/wlan_modules/os/linux/tools/
CFLAGS += -I$(PWD)/../../../drivers/wlan_modules/os/linux/src/
CFLAGS += -I$(PWD)/../../../drivers/wlan_modules/direct_attach/lmac/ath_dev/
LDFLAGS += -pie
else
CFLAGS += -I$(WLAN_TOP)/include
CFLAGS += -I$(WLAN_TOP)/include/libnl3/
LDFLAGS += -pie -lnl-3 -lnl-genl-3 -lxml2 -lqca_nl80211_wrapper
endif


CFLAGS += -I$(PWD) -I$(PWD)/../src -Wall
CFLAGS += -D_GNU_SOURCE
CFLAGS += -L $(PWD)/../src
CFLAGS += -fpie

ifeq ($(strip ${BUILD_X86}),y)
CFLAGS += -DBUILD_X86=1
Bx86_LIB= -lbsd
endif

all:
	@echo -e "\nBuilding ACFG TESTAPP..."
	$(CC) $(SRCS) $(CFLAGS) $(LDFLAGS) $(COPTS) -o $(ACFG_BINARY) -l$(LIB_OUT) $(Bx86_LIB)

clean:
	@echo -e "\nCleaning ACFG TESTAPP..."
	rm -f $(ACFG_BINARY)
	rm -f $(OBJS)

