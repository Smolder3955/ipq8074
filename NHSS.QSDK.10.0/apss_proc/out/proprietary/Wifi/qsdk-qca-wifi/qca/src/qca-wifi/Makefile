#
# Copyright (c) 2014, 2018 Qualcomm Atheros, Inc..
#
# All Rights Reserved.
# Qualcomm Atheros Confidential and Proprietary.
#
CURR_DIR=$(shell pwd)
FWPROTOCOL_SUPPORT:=$(CURR_DIR)/fwprotocol/support
FWPROTOCOL_TOOLS:=$(CURR_DIR)/fwprotocol/tools
include config.mk

ifeq ($(INSTALL_ROOT),)
INSTALL_ROOT:=$(CURR_DIR)/root
endif


#########################################################
############ WLAN DRIVER BUILD CONFIGURATION ############
#########################################################

$(warning "MAKE=$(MAKE)")
$(warning "MAKEARCH=$(MAKEARCH)")
$(warning "TARGET=$(TARGET)")
$(warning "TOPDIR=$(TOPDIR)")
$(warning "INSTALL_ROOT=$(INSTALL_ROOT)")

LINUX_KARCH:=$(ARCH)

########################################################
############## WLAN CONFIG OVERRIDE FACILITY ###########
########################################################
ifneq ($(QCAWLAN_MAKEOPTS_EXTRAS),)
	QCAWLAN_MAKEOPTS+=$(QCAWLAN_MAKEOPTS_EXTRAS)
$(warning "QCAWLAN_MAKEOPTS_EXTRAS=$(QCAWLAN_MAKEOPTS_EXTRAS)")
endif

$(warning "ARCH=$(ARCH)")

ifeq ($(ARCH),mips)
	HAL_TARGET:=mipsisa32-be-elf
endif
ifeq ($(ARCH),arm)
	HAL_TARGET:=armv4-le-elf
QCAWLAN_MAKEOPTS+=BUILD_ARM=y
endif
ifeq ($(ARCH),aarch64)
	HAL_TARGET:=arm64-elf
LINUX_KARCH:=arm64
QCAWLAN_MAKEOPTS+=BUILD_ARM=y
endif
ifeq ($(ARCH),x86)
	HAL_TARGET:=i386-elf
endif
ifeq ($(QCA_PLATFORM),ar71xx)
	QCAWLAN_MAKEOPTS+= \
		BUS=dual \
	        AH_CAL_IN_DRAM_FROM_HOST=1 \
	        AH_CAL_RADIOS_AHB=1 \
		AH_CAL_RADIOS_PCI=1 \
	        AH_SUPPORT_SCORPION=1 \
            AH_SUPPORT_DRAGONFLY=1 \
            AH_SUPPORT_HONEYBEE=1
endif
ifeq ($(QCA_PLATFORM),ipq806x)
	QCAWLAN_MAKEOPTS+=BUILD_ARM_MSM=y \
		AH_CAL_RADIOS_PCI=3 \
                AH_CAL_LOCATIONS_PCI=0,0x4000,0x8000
endif
ifeq ($(CONFIG_WIFI_TARGET_WIFI_3_0),y)
	WIFI_TARGET_3_0 = 1
	QCAWLAN_MAKEOPTS+=WIFI_TARGET_3_0=1
else
	WIFI_TARGET_3_0 = 0
	QCAWLAN_MAKEOPTS+=WIFI_TARGET_3_0=0
endif
ifeq ($(CONFIG_WIFI_TARGET_WIFI_2_0),y)
	WIFI_TARGET_2_0 = 1
	QCAWLAN_MAKEOPTS+=WIFI_TARGET_2_0=1
else
	WIFI_TARGET_2_0 = 0
	QCAWLAN_MAKEOPTS+=WIFI_TARGET_2_0=0
endif

ifeq ($(CONFIG_WIFI_IPQ_MEM_PROFILE),512)
	WIFI_MEM_PROFILE_512 = 1
	QCAWLAN_MAKEOPTS+=WIFI_MEM_PROFILE_512=1
endif
# For enabling parallel compilation with 8 simultaneous jobs :
# export N_DCTHREADS=8
# if you see compilation error(s) you can either unset it or set it to 1
# unset N_DCTHREADS
#   or
# export  N_DCTHREADS=1
N_DCTHREADS?=1
MAKEARCH:=$(MAKE) -j$(N_DCTHREADS) ARCH=$(LINUX_KARCH) CROSS_COMPILE=$(TARGET_CROSS)

QCAWLAN_MAKEOPTS+= \
	TOOLPATH= \
	TOOLPREFIX=$(TARGET_CROSS) \
	NO_SIMPLE_CONFIG=1 \
	USE_PLATFORM_FRAMEWORK=1 \
	TARGET=$(HAL_TARGET) \
	BIG_ENDIAN_HOST=$(BIG_ENDIAN_HOST) \
	ARCH=$(LINUX_KARCH) \
	KERNELARCH=$(LINUX_KARCH) \
	KERNELVER=$(LINUX_VERSION) \
	FIRMWARE_REL_PATH=offload \
	PERF_PWR_OFFLOAD_DIR_PATH=$(CURR_DIR)/offload \
	QCA_NSS_INCLUDE=$(STAGING_DIR)/usr/include/qca-nss-drv \
	KBUILDPATH=$(LINUX_SRC_DIR) \
	KERNELPATH=$(LINUX_DIR)

src_prepare:
	mkdir -p  $(CURR_DIR)/halphy_tools
	mkdir -p $(CURR_DIR)/offload/include/wifistats/
	$(warning "CONFIG_WIFI_TARGET_WIFI_2_0=$(WIFI_TARGET_2_0)")
	$(warning "CONFIG_WIFI_TARGET_WIFI_3_0=$(WIFI_TARGET_3_0)")
	$(warning "FW_HDR_INCLUDE_PATH=$(FW_HDR_INCLUDE_PATH)")
ifeq ($(WIFI_TARGET_3_0),1)
#moving of header files is a temporary fix, should be removed once dependencies are resolved
	rm -f $(FW_HDR_INCLUDE_PATH)/fw/a_osapi.h
	rm -f $(FW_HDR_INCLUDE_PATH)/fw/athendpack.h
	cp -af $(FW_HDR_INCLUDE_PATH)/fw/* $(CURR_DIR)/offload/include/
	cp -af $(FW_HDR_INCLUDE_PATH)/hw/* $(CURR_DIR)/offload/include/
	cp -af $(FW_COMMON_INCLUDE_PATH)/qca8074/* $(CURR_DIR)/offload/include/qca8074/
ifeq ($(ENABLE_6018_HEADER),1)
	cp -af $(FW_COMMON_INCLUDE_PATH)/qca6018/ $(CURR_DIR)/offload/include/.
endif
	cp -af $(FW_INTERFACE_TOOLS_PATH)/wifistats/* $(CURR_DIR)/offload/include/wifistats/
	rm -f $(CURR_DIR)/offload/include/wlan_tgt_def_config.h
	rm -f $(CURR_DIR)/offload/include/wlan_tgt_def_config_hl.h
else
	cp -af $(FW_INCLUDE_PATH)/* $(CURR_DIR)/offload/include/
endif

ifeq ($(WIFI_TARGET_2_0),1)
	mkdir -p $(CURR_DIR)/offload/include/legacy
	cp -af $(FW_INCLUDE_PATH)/dbglog_rtt_host.h $(CURR_DIR)/offload/include/legacy/
	cp -af $(FW_INCLUDE_PATH)/dbglog_host_gen.h $(CURR_DIR)/offload/include/legacy/
	cp -af $(FW_INCLUDE_PATH)/wmi.h $(CURR_DIR)/offload/include/legacy/
	cp -af $(FW_INCLUDE_PATH)/wmi_services.h $(CURR_DIR)/offload/include/legacy/
	cp -af $(FW_INCLUDE_PATH)/wlan_wow.h $(CURR_DIR)/offload/include/legacy/
	cp -af $(FW_INCLUDE_PATH)/wmi_unified.h $(CURR_DIR)/offload/include/legacy/
	cp -af $(FW_INCLUDE_PATH)/dbglog.h $(CURR_DIR)/offload/include/legacy/
	cp -af $(FW_INCLUDE_PATH)/dbglog_id.h $(CURR_DIR)/offload/include/legacy/
	cp -af $(FW_INCLUDE_PATH)/wlan_module_ids.h $(CURR_DIR)/offload/include/legacy/
	cp -af $(FW_INCLUDE_PATH)/rtt_interface.h $(CURR_DIR)/offload/include/legacy/
	cp -af $(FW_INCLUDE_PATH)/wmix.h $(CURR_DIR)/offload/include/legacy/
	cp -af $(FW_INCLUDE_PATH)/htt.h $(CURR_DIR)/offload/include/legacy/
#htt_deps.h not present in fwcommon, hence copied only in offload/include. No conflict
	cp -af $(FW_INCLUDE_PATH)/htt_deps.h $(CURR_DIR)/offload/include/
#targaddrs.h doesnt need alternate path currently, hence copied only in offload/include.
	cp -af $(FW_INCLUDE_PATH)/targaddrs.h $(CURR_DIR)/offload/include/
	cp -af $(FW_INCLUDE_PATH)/wlan_defs.h $(CURR_DIR)/offload/include/legacy/
	mkdir -p $(CURR_DIR)/offload/include/legacy/QCA9984
	cp -af $(FW_INCLUDE_PATH)/QCA9984/* $(CURR_DIR)/offload/include/legacy/QCA9984/
	mkdir -p $(CURR_DIR)/offload/include/legacy/QCA9888
	cp -af $(FW_INCLUDE_PATH)/QCA9888/* $(CURR_DIR)/offload/include/legacy/QCA9888/
	mkdir -p $(CURR_DIR)/offload/include/legacy/IPQ4019
	cp -af $(FW_INCLUDE_PATH)/IPQ4019/* $(CURR_DIR)/offload/include/legacy/IPQ4019/
	mkdir -p $(CURR_DIR)/offload/include/legacy/AR9888
	cp -af $(FW_INCLUDE_PATH)/AR9888/* $(CURR_DIR)/offload/include/legacy/AR9888/
	mkdir -p $(CURR_DIR)/offload/include/legacy/AR900B
	cp -af $(FW_INCLUDE_PATH)/AR900B/* $(CURR_DIR)/offload/include/legacy/AR900B/
	$(warning "FW_INCLUDE_PATH=$(FW_INCLUDE_PATH)")
endif
ifeq ($(WIFI_TARGET_3_0),1)
	mkdir -p $(CURR_DIR)/offload/include/fwcommon
	mv -f $(CURR_DIR)/offload/include/wmi.h $(CURR_DIR)/offload/include/fwcommon/
	mv -f $(CURR_DIR)/offload/include/wmi_services.h $(CURR_DIR)/offload/include/fwcommon/
	mv -f $(CURR_DIR)/offload/include/wmi_unified.h $(CURR_DIR)/offload/include/fwcommon/
	mv -f $(CURR_DIR)/offload/include/wmix.h $(CURR_DIR)/offload/include/fwcommon/
	mv -f $(CURR_DIR)/offload/include/wmi_tlv_defs.h $(CURR_DIR)/offload/include/fwcommon/
	mv -f $(CURR_DIR)/offload/include/wmi_tlv_helper.h $(CURR_DIR)/offload/include/fwcommon/
	mv -f $(CURR_DIR)/offload/include/wmi_version.h $(CURR_DIR)/offload/include/fwcommon/
	mv -f $(CURR_DIR)/offload/include/wlan_defs.h $(CURR_DIR)/offload/include/fwcommon/
	mv -f $(CURR_DIR)/offload/include/dbglog.h $(CURR_DIR)/offload/include/fwcommon/
	mv -f $(CURR_DIR)/offload/include/dbglog_id.h $(CURR_DIR)/offload/include/fwcommon/
	#Temporary WAR kept legacy wlan_defs in offload/include due to htt_deps.h dependency
	cp -af $(FW_INCLUDE_PATH)/wlan_defs.h $(CURR_DIR)/offload/include/
	mv -f $(CURR_DIR)/offload/include/htt.h $(CURR_DIR)/offload/include/fwcommon/

	#cp -af $(CURR_DIR)/hal/linux/KbuildQDF cmn_dev/qdf/Kbuild
endif
	cp -af $(FW_INCLUDE_PATH)/sw_version.h $(CURR_DIR)/offload/include/
	cp -af $(HALPHY_TOOLS_PATH)/* $(CURR_DIR)/halphy_tools/

env_prepare:
	@echo "============================PREPARING==========================="
	@echo $(QCAWLAN_MAKEOPTS)
	mkdir -p $(CURR_DIR)/root/usr/www/cgi-bin
	mkdir -p $(CURR_DIR)/root/sbin

hal_build:
	@echo "========================BUILDING HAL==========================="
	($(foreach var,$(strip $(QCAWLAN_MAKEOPTS)),export $(var);) \
	    $(MAKE) -j$(N_DCTHREADS) -C $(CURR_DIR)/direct_attach/hal/linux TARGET=$(HAL_TARGET))
	($(foreach var,$(strip $(QCAWLAN_MAKEOPTS)),export $(var);) \
	    $(MAKE) -j$(N_DCTHREADS) -C $(CURR_DIR)/direct_attach/hal/linux TARGET=$(HAL_TARGET) release)

driver_build:
	@echo "===========================BUILDING DRIVER================================="
	($(foreach var,$(strip $(QCAWLAN_MAKEOPTS)),export $(var);) \
		$(MAKEARCH) -C $(CURR_DIR)/os/linux TARGET=$(HAL_TARGET))

driver_installonly:
	cp -af $(foreach module,$(QCAWLAN_MODULE_LIST),$(module)) $(INSTALL_ROOT_DRV)
	@echo "===========================INSTALL DRIVER DONE====================================="

driver_install: driver_build driver_installonly
	@echo "============================DRIVER INSTALLED================================"
tools_build:
	@echo "===========================BUILDING TOOLS================================="
	($(foreach var,$(strip $(QCAWLAN_MAKEOPTS)),export $(var);) \
	    $(MAKEARCH) -C $(CURR_DIR)/os/linux/tools \
	    QCACFLAGS="$(TARGET_CFLAGS) -Wall -Wno-error"  \
	    QCALDFLAGS="-Wl,--gc-sections $(TARGET_LDFLAGS) -Wall -Wno-error" \
	        INSTALL_ROOT=$(CURR_DIR)/root ath_tools wlanconfig)
	($(foreach var,$(strip $(QCAWLAN_MAKEOPTS)),export $(var);) \
		$(MAKEARCH) -C $(CURR_DIR)/halphy_tools/host/testcmd_tlv \
		-f makefile.linux WLAN_TOP=$(CURR_DIR))

tools_installonly:
	@echo "==============================INSTALLING TOOLS==================================="
	$(INSTALL_BIN) $(foreach tool,$(QCAWLAN_TOOL_LIST),$(CURR_DIR)/os/linux/tools/$(tool)) $(INSTALL_BIN_DEST)
	$(INSTALL_BIN) $(CURR_DIR)/halphy_tools/host/testcmd_tlv/Linux/athtestcmd $(INSTALL_BIN_DEST)
	$(INSTALL_DATA) $(CURR_DIR)/halphy_tools/host/testcmd_tlv/Linux/*.so $(INSTALL_LIB_DEST)
	-$(INSTALL_DATA) $(CURR_DIR)/os/linux/tools/*.so $(INSTALL_LIB_DEST)

tools_install: tools_build tools_installonly

install_headers:
	@echo "================================INSTALL HEADERS====================================="
	cp -af $(foreach header,$(QCAWLAN_HEADERS),$(CURR_DIR)/$(header)) $(INSTALL_DEST)

ifeq ($(DA_SUPPORT),1)
all_build: env_prepare hal_build driver_build tools_build
else
all_build: env_prepare driver_build tools_build
endif

	@echo "========================Finished qca-wifi-10.4 build================================"

all_installonly: driver_installonly tools_installonly
	@echo "========================Finished qca-wifi-10.4 install=============================="

all_install: all_build all_installonly

