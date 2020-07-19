ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CC= $(CROSS)gcc

ifndef ATHEROSPATH
ATHEROSPATH := $(TOPDIR)/drivers/wlan_modules
endif

OS_INC_PATH := $(ATHEROSPATH)/os/linux/include
HAL_PATH := $(ATHEROSPATH)/hal
ADF_PATH := $(ATHEROSPATH)/adf
ASF_PATH := $(ATHEROSPATH)/asf
LMAC_PATH := $(ATHEROSPATH)/lmac
UMAC_PATH := $(ATHEROSPATH)/umac
INCS+= -I${OS_INC_PATH} -I${HAL_PATH} -I${ATHEROSPATH} -I.

CFLAGS= ${INCS} -MMD -O2 -Wall -g -lrt -lm -Werror

ALL= wpc

all: ${ALL}

wpc: wpc.c
	${CC} -o wpc ${CFLAGS} ${TARGET_CFLAGS} ${LDFLAGS} ${TARGET_LDFLAGS} -lpthread -ldl wpc.c wpc_siface.c wpc_diface.c wpc_me.c wpc_mgr.c wpc_lib.c
clean: 
	rm -f ${ALL} 
