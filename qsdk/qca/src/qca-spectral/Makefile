#
# Makes spectral apps
#

ifeq ($(obj),)
obj := .
endif

# Default OS is linux
ifeq ($(OS),)
OS := linux
endif

# Important note: Values of CC, AR and LD should be passed to this script.

# Path defines...
SPECTRAL_DRIVER_PATH	:= $(ATHEROSPATH)/lmac/spectral
ADF_INC					:= ${ATHEROSPATH}/adf/include
ADF_OS_INC				:= ${ATHEROSPATH}/adf/include/os
ASF_INC					:= ${ATHEROSPATH}/asf/include
ATH_DEV_INC				:= ${ATHEROSPATH}/lmac/ath_dev
ATH_DFS_INC				:= ${ATHEROSPATH}/lmac/dfs
UMAC_INC				:= ${ATHEROSPATH}/umac/include
COMMON_INC				:= ./common
HAL_INC					:= ${ATHEROSPATH}/hal
CLASSIFIER_SRC			:= ./common
CLASSIFIER_INC			:= ./common
BINDIR					= /usr/local/bin
DEPTH					:= ../../drivers/wlan
TOOLS_HEADER_INC_PATH	:= ../../drivers/wlan_modules/os/linux/include
WLAN_INC_PATH			:= ../../drivers/wlan_modules/include
OL_WLAN_INC_PATH	    := $(ATHEROSPATH)/../firmware/include/
OL_WLAN_HINC_PATH	    := $(ATHEROSPATH)/../firmware/host/include/

# Spectral
ifeq ($(strip ${ATH_SUPPORT_SPECTRAL}),1)
ALL+= athssd
SPECTRAL_TOOLS+= athssd
endif

# Diagtools, enabled for AR900B (Beeliner)
ifeq ($(strip ${ATH_TGT_TYPE}),AR900B)
ALL+= diagtools
SPECTRAL_TOOLS+= diagtools
endif

ALL+= libath_classifier.a

all: ${ALL}

# Add necessary include path
INCS+= -I$(ATHEROSPATH) -I${HAL}/${OS} -I${HAL_INC} -I${COMMON_INC}
INCS+= -I${CLASSIFIER_INC} -I${ADF_INC} -I${ADF_OS_INC} -I${ASF_INC}
INCS+= -I${SPECTRAL_DRIVER_PATH} -I${ATH_DEV_INC} -I${ATH_DFS_INC} -I.
INCS+= -I${TOOLS_HEADER_INC_PATH} -I${WLAN_INC_PATH} -I${OL_WLAN_INC_PATH} -I${OL_WLAN_HINC_PATH}

# Update CFLAGS
CFLAGS+= ${INCS} ${COPTS} -Wall -Werror

ifeq (${BUILD_STATIC}, y)
LDFLAGS= -static
endif

ifeq (${BUILD_DEBUG}, y)
CFLAGS+= -g
endif

ifeq (${BUILD_OPTIMIZED}, y)
CFLAGS += -Os -pipe -mips32r2 -mtune=mips32r2 -funit-at-a-time
endif

ifeq ($(strip ${SPECTRAL_SUPPORT_CFG80211}),1)
CFLAGS += -DSPECTRAL_SUPPORT_CFG80211
LDFLAGS += -lpthread
endif

CFLAGS += -fpie
LDFLAGS += -pie

libath_classifier.a: ath_classifier.o
	${AR} -cvq libath_classifier.a ath_classifier.o

ath_classifier.o: ath_classifier.c
	${CC} -c -o ath_classifier.o ${CFLAGS} ${LDFLAGS} ath_classifier.c

athssd: ath_ssd.c ${CLASSIFIER_SRC}/classifier.c ath_ssd_cmds.c libath_classifier.a
	${CC} -o athssd ${CFLAGS} ${LDFLAGS} ath_ssd.c ath_ssd_cmds.c libath_classifier.a -lqca_nl80211_wrapper -lm

diagtools: logadc.c logspectral.c
	make -f Makefile.logspectraladc

clean:
	@rm -rf athssd libath_classifier.a

athssd_clean:
	@rm -rf athssd
