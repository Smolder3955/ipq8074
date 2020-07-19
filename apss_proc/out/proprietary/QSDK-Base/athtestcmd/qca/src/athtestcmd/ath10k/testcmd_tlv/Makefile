CC =$(TOOLPREFIX)gcc
LD =$(TOOLPREFIX)ld

TLV2P0_ROOT = ../tlv2p0

CFLAGS +=  -Wall -DLINUX -DLinux -I../os/linux/tools/systemtools  -I../utf_utilities/genParmTemplate/.output  -I../utf_utlities -DTEST_STANDALONE -D_FOR_HOST_TESTING -D_PARSER_BIN_CMD_STREAM_API_ONLY -DUSED_BY_HOST -I../utf_utilities/include/ -D_HOST_SIM_TESTING -D_DEBUG -D_CAL_PWR_AS_API -I../utf_utilities/libtlvutil/ -I../eepromUtil -I../include/ -I../tlv2p0/include/ -I../os/linux/include/ -I../include/tlv1

CFLAGS += -DFCC_INCLUDED -D_OTA_TOOL -DSUPPORT_VHT -DARCH_BIG_ENDIAN

ifeq ($(TARGET),AR9888) # {
CFLAGS += -D_HOSTTOOLS_AR98XX
endif # }

COMMON_DIR      = ../utf_utilities/common

VPATH   := ../eepromUtil:../utf_utilities/genParmTemplate/.output:$(COMMON_DIR):../utf_utilities/verifyCmdStream
#LDFLAGS+=$(LIBS)
LDFLAGS += -L../utf_utilities/libtlvutil/Linux/ -ltlvutil 
LDFLAGS += -lrt #-L../../../../staging_dir/toolchain-arm_cortex-a7_gcc-4.8-linaro_uClibc-0.9.33.2_eabi/lib/ -lrt
LDFLAGS += -L../utf_utilities/libtlvutil/genParmTemplate/outputTemplate/Linux/ -ltlvtemplate
#LDFLAGS += -L./addnl_inst/ -lrsnrpz -lnrp
LDFLAGS += -L$(TLV2P0_ROOT)/tlvcmdrsp/Linux/ -ltlvcmdrsp
LDFLAGS += -L$(TLV2P0_ROOT)/cmdParser/Linux/ -ltlvparser
#LDFLAGS        += -L$(TLV2P0_ROOT)/TLVCmdRsp/Linux/ -ltlv-cmdrsp
LDFLAGS += -L$(TLV2P0_ROOT)/cmdEncoder/Linux/ -ltlvencoder
LDFLAGS += -L../../../../staging_dir/target-arm_cortex-a7_uClibc-0.9.33.2_eabi/usr/lib/ -lnl-tiny

ifeq ($(A_SIM_UTF), y)
CFLAGS += -m32 -DUTFSIM
LDFLAGS += -m32
endif

LIBTLV_DIR  = ../utf_utilities/libtlvutil/

EXTRA_SRCS := testUtil.c eepromUtil_ar6004.c addnl_inst/rsnrpzBurstAv.c

ifeq ($(OS),)
OS := Linux
endif

ifeq ($(DEBUG),1)
    CFLAGS += -g
endif

# hack to workaround p4 views
QCAWIFI = ../../qca-wifi-10.4
CFLAGS += -I$(QCAWIFI)/os/linux/include/ -I$(QCAWIFI)/include/ -I$(QCAWIFI)/lmac/ath_dev/



BIN_TC   = $(OS)/athtestcmd
SRCS_TC := athtestcmd.c processRsp.c ../os/linux/tools/systemtools/os_if_ath10k.c
OBJS_TC := athtestcmd.o processRsp.o ../os/linux/tools/systemtools/os_if_ath10k.o
OBJS_TC += $(LDLIBS)

BIN_TXPWR   = $(OS)/calTxPwr.out
SRCS_TXPWR := mInst.c calTxPwr.c $(EXTRA_SRCS)
OBJS_TXPWR :=  $(patsubst %.c,$(OS)/%.o,$(SRCS_TXPWR))

#all: dirs $(BIN_TC) $(BIN_TXPWR)
all: dirs $(BIN_TC)
#all: dirs $(BIN_TXPWR)

dirs:
	mkdir -p $(OS)
	mkdir -p $(OS)/addnl_inst
	cd $(LIBTLV_DIR);$(MAKE) -f makefile.Linux clean
	cd $(LIBTLV_DIR);$(MAKE) -f makefile.Linux all
	cp ../utf_utilities/libtlvutil/Linux/libtlvutil.so $(OS)
	cp ../utf_utilities/libtlvutil/genParmTemplate/outputTemplate/Linux/libtlvtemplate.so $(OS)
	cd $(TLV2P0_ROOT)/tlvcmdrsp;$(MAKE) -f makefile.Linux clean
	cd $(TLV2P0_ROOT)/tlvcmdrsp;$(MAKE) -f makefile.Linux all
	cd $(TLV2P0_ROOT)/cmdEncoder;$(MAKE) -f makefile.Linux clean
	cd $(TLV2P0_ROOT)/cmdEncoder;$(MAKE) -f makefile.Linux all
	cd $(TLV2P0_ROOT)/cmdParser;$(MAKE) -f makefile.Linux clean
	cd $(TLV2P0_ROOT)/cmdParser;$(MAKE) -f makefile.Linux all
	cp $(TLV2P0_ROOT)/cmdParser/Linux/libtlvparser.so $(OS)
	cp $(TLV2P0_ROOT)/cmdEncoder/Linux/libtlvencoder.so $(OS)
	cp $(TLV2P0_ROOT)/tlvcmdrsp/Linux/libtlvcmdrsp.so $(OS)
	#cp ../../../../build_dir/target-arm_cortex-a7_uClibc-0.9.33.2_eabi/libnl-tiny-0.1/libnl-tiny.so $(OS)

$(BIN_TC): $(OBJS_TC)
	$(CC) $(CFLAGS) $(OBJS_TC) $(LDFLAGS) -o $@

$(BIN_TXPWR): $(OBJS_TXPWR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS_TXPWR)

$(OS)/%.o : %.c
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -rf $(OS)
	cd $(LIBTLV_DIR);$(MAKE) -f makefile.Linux clean

.PHONY:
	clean all
