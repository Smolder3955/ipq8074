CC=$(TOOLPREFIX)gcc
AR=$(TOOLPREFIX)ar

CFLAGS += -I$(PWD)/../include

ifndef QSDK_ACFG
CFLAGS += -I$(PWD)/../../../drivers/wlan_modules/include
CFLAGS += -I$(PWD)/../../../drivers/wlan_modules/cmn_dev/qdf/inc
CFLAGS += -I$(PWD)/../../../drivers/wlan_modules/cmn_dev/qdf/linux/src/
CFLAGS += -I$(PWD)/../../../drivers/wlan_modules/os/linux/tools/
CFLAGS += -I$(PWD)/../../../drivers/wlan_modules/os/linux/src/
CFLAGS += -I$(PWD)/../../../drivers/wlan_modules/direct_attach/lmac/ath_dev/
QCATOOLS_LIB = $(PWD)/../../../drivers/wlan_modules/os/linux/tools/qcatools_lib.o
else
CFLAGS += -I$(WLAN_TOP)/include
CFLAGS += -I$(WLAN_TOP)/include/libnl3/
QCATOOLS_LIB = $(WLAN_TOP)/include/qcatools_lib.o
endif

CFLAGS += -I$(PWD)
CFLAGS += -MMD
CFLAGS += -Wall -Werror -O2 -Wsign-compare -Wpointer-sign -Wunused-parameter -Wtype-limits -Wmissing-field-initializers -Wunused-variable -Wstrict-aliasing -Wtype-limits -Wuninitialized -g
CFLAGS += -D_GNU_SOURCE
CFLAGS += -fpie
LDFLAGS += -pie -lnl-3 -lnl-genl-3 -lxml2 -lqca_nl80211_wrapper

ifeq ($(strip ${ACFG_PARTIAL_OFFLOAD}),1)
CFLAGS += -DACFG_PARTIAL_OFFLOAD
endif

OBJ_OUT=acfg_api.o
LIB_OUT=acfg
LIB_TYPE=rcs
ACFG_BINARY=acfg_tool
ACFG_LIB=libacfg.a
ACFG_TEST_DIR=../acfg_test_app
ACFG_CONF_FILE=acfg_sample.conf

FILES=$(shell ls *.c)
OBJS=$(patsubst %.c,%.o,$(FILES))
OBJS+=$(QCATOOLS_LIB)

ifeq ($(strip ${BUILD_X86}),y)
CFLAGS += -DBUILD_X86=1
Bx86_LIB = -lbsd
endif

ifeq ($(strip ${UMAC_SUPPORT_ACFG}),1)
CFLAGS += -DUMAC_SUPPORT_ACFG=1
endif

%.o: %.c
	$(CC) -c $(CFLAGS) $(LDFLAGS) $(COPTS) $< -o $@ $(Bx86_LIB)

export LIB_OUT ACFG_BINARY

all: $(OBJS)
#$(CC) $(CFLAGS) -c $(FILES) -o $(OBJ_OUT)
#$(AR) $(LIB_TYPE) $(ACFG_LIB) $(OBJ_OUT) 
	$(AR) $(LIB_TYPE) $(ACFG_LIB) $(OBJS)
	make -C $(ACFG_TEST_DIR) all 

install:
	-cp -v $(ACFG_LIB) $(DESTDIR)/lib
	-cp -v $(ACFG_TEST_DIR)/$(ACFG_BINARY) $(DESTDIR)/sbin
	-cp -v $(ACFG_TEST_DIR)/$(ACFG_CONF_FILE) $(DESTDIR)/etc
	-cp -v acfg_common.conf $(DESTDIR)/etc


clean:
	rm -rvf *.o *.a
	make -C $(ACFG_TEST_DIR) clean
    
