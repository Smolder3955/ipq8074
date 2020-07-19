#
## Copyright (c) 2014, 2017, 2019 Qualcomm Innovation Center, Inc.
# All Rights Reserved.
# Confidential and Proprietary - Qualcomm Innovation Center, Inc.
#
# 2014 Qualcomm Atheros, Inc..
#
# All Rights Reserved.
# Qualcomm Atheros Confidential and Proprietary.
#

LINUX_KMOD_SUFFIX:=ko
QCAWLAN_MODULE_LIST:=
PWD:=$(shell pwd)
DA_SUPPORT:=0
# These two functions are used to define options based on WLAN config
if_opt_set=       $(if $(filter $(1)=1,$(QCAWLAN_MAKEOPTS)),$(2),)
if_opt_clear=     $(if $(filter $(1)=0,$(QCAWLAN_MAKEOPTS)),$(2),)

# Use the function below to add driver opts depending on menuconfig values
append_if_notnull=QCAWLAN_MAKEOPTS+=$(if $(call qstrip,$(1)),$(2),$(3))

ifneq ($(BUILD_VARIANT),)
ifneq ($(DRIVER_PATH),)
QCAWLAN_MAKEOPTS:=$(shell cat $(DRIVER_PATH)/os/linux/configs/config.wlan.$(subst -,.,$(BUILD_VARIANT)))
else
QCAWLAN_MAKEOPTS:=$(shell cat $(PWD)/os/linux/configs/config.wlan.$(subst -,.,$(BUILD_VARIANT)))
endif
endif

ifeq ($(CONFIG_WIFI_TARGET_WIFI_3_0),y)
QCAWLAN_MAKEOPTS+=WIFISTATS_TOOL_SUPPORT=1
endif

QCAWLAN_HEADERS:= \
   cmn_dev/qdf/inc/qdf_types.h \
   cmn_dev/qdf/inc/qdf_status.h \
   cmn_dev/qdf/linux/src/i_qdf_types.h \
   offload/include/a_types.h \
   offload/include/athdefs.h \
   offload/include/wlan_defs.h \
   offload/include/ol_txrx_stats.h \
   cmn_dev/dp/inc/cdp_txrx_stats_struct.h \
   include/ieee80211_defines.h \
   include/_ieee80211.h \
   include/ieee80211.h \
   include/ieee80211_wnm_proto.h \
   include/ieee80211_band_steering_api.h \
   include/if_upperproto.h \
   include/compat.h \
   include/wlan_opts.h \
   include/sys/queue.h \
   include/ieee80211_phytype.h \
   include/ext_ioctl_drv_if.h \
   include/ol_if_thermal.h \
   include/qwrap_structure.h \
   include/ieee80211_rrm.h \
   direct_attach/hal/linux/ah_osdep.h \
   os/linux/include/ath_ald_external.h \
   os/linux/include/ieee80211_external.h \
   os/linux/include/ieee80211_ioctl.h \
   os/linux/include/ieee80211_ev.h \
   os/linux/include/ieee80211_wpc.h \
   os/linux/tools/athrs_ctrl.h \
   direct_attach/lmac/ath_dev/if_athioctl.h \
   cmn_dev/spectral/dispatcher/inc/spectral_ioctl.h \
   include/acfg_wsupp_api.h \
   include/acfg_event_types.h \
   include/acfg_event.h \
   include/acfg_api_types.h \
   include/appbr_types.h \
   include/bypass_types.h \
   os/linux/src/ath_papi.h \
   include/ieee80211_parameter_api.h \
   cmn_dev/umac/dfs/dispatcher/inc/wlan_dfs_ioctl.h \
   umac/airtime_fairness/dispatcher/inc/wlan_atf_utils_defs.h \
   os/linux/src/ath_ssid_steering.h \
   os/linux/src/cfg80211_external.h \
   os/linux/tools/qcatools_lib.h \
   os/linux/tools/qcatools_lib.o \
   cmn_dev/os_if/linux/qca_vendor.h \
   include/ieee80211_external_config.h

#########################################################
############ WLAN DRIVER BUILD CONFIGURATION ############
#########################################################
# Module list
# This list is filled dynamically based on the WLAN configuration
# It depends on the content of the wlan config file (.profile)
QCAWLAN_MODULE_LIST+=$(strip $(call if_opt_set, WIFI_MEM_MANAGER_SUPPORT, \
	$(PKG_BUILD_DIR)/os/linux/mem/mem_manager.$(LINUX_KMOD_SUFFIX)))
QCAWLAN_MODULE_LIST+=$(PKG_BUILD_DIR)/cmn_dev/qdf/qdf.$(LINUX_KMOD_SUFFIX)
QCAWLAN_MODULE_LIST+=$(PKG_BUILD_DIR)/asf/asf.$(LINUX_KMOD_SUFFIX)
QCAWLAN_MODULE_LIST+=$(PKG_BUILD_DIR)/umac/umac.$(LINUX_KMOD_SUFFIX)
QCAWLAN_MODULE_LIST+=$(strip $(call if_opt_set, WLAN_SPECTRAL_ENABLE, \
	$(PKG_BUILD_DIR)/cmn_dev/spectral/qca_spectral.$(LINUX_KMOD_SUFFIX)))
ifeq ($(DA_SUPPORT),1)
QCAWLAN_MODULE_LIST+=$(PKG_BUILD_DIR)/direct_attach/os/linux/ath_hal/ath_hal.$(LINUX_KMOD_SUFFIX)
QCAWLAN_MODULE_LIST+=$(PKG_BUILD_DIR)/direct_attach/lmac/ratectrl/ath_rate_atheros.$(LINUX_KMOD_SUFFIX)
QCAWLAN_MODULE_LIST+=$(strip $(call if_opt_set, ATH_SUPPORT_TX99, \
	$(PKG_BUILD_DIR)/direct_attach/lmac/tx99/hst_tx99.$(LINUX_KMOD_SUFFIX)))
QCAWLAN_MODULE_LIST+=$(PKG_BUILD_DIR)/direct_attach/lmac/ath_dev/ath_dev.$(LINUX_KMOD_SUFFIX)
QCAWLAN_MODULE_LIST+=$(PKG_BUILD_DIR)/direct_attach/qca_da/qca_da.$(LINUX_KMOD_SUFFIX)
endif
QCAWLAN_MODULE_LIST+=$(PKG_BUILD_DIR)/qca_ol/qca_ol.$(LINUX_KMOD_SUFFIX)
ifeq ($(CONFIG_WIFI_TARGET_WIFI_3_0),y)
QCAWLAN_MODULE_LIST+=$(PKG_BUILD_DIR)/qca_ol/wifi3.0/wifi_3_0.$(LINUX_KMOD_SUFFIX)
endif
ifeq ($(CONFIG_WIFI_TARGET_WIFI_2_0),y)
QCAWLAN_MODULE_LIST+=$(PKG_BUILD_DIR)/qca_ol/wifi2.0/wifi_2_0.$(LINUX_KMOD_SUFFIX)
endif
QCAWLAN_MODULE_LIST+=$(PKG_BUILD_DIR)/lmac/ath_pktlog/ath_pktlog.$(LINUX_KMOD_SUFFIX)
QCAWLAN_MODULE_LIST+=$(strip $(call if_opt_set, UNIFIED_SMARTANTENNA, \
	$(PKG_BUILD_DIR)/smartantenna/smart_antenna.$(LINUX_KMOD_SUFFIX)))
QCAWLAN_MODULE_LIST+=$(strip $(call if_opt_set, ATH_SW_WOW_SUPPORT, \
	$(PKG_BUILD_DIR)/wow/sw_wow.$(LINUX_KMOD_SUFFIX)))

#########################################################
################# BUILD/INSTALL RULES ###################
#########################################################
QCAWLAN_TOOL_LIST:= 80211stats athstats athstatsclr apstats pktlogconf pktlogdump wifitool wlanconfig thermaltool wps_enhc exttool assocdenialnotify athkey qca_gensock
QCAWLAN_TOOL_LIST+= $(call if_opt_set, ATH_SUPPORT_DFS, radartool)
QCAWLAN_TOOL_LIST+= $(call if_opt_set, WLAN_SPECTRAL_ENABLE, spectraltool)
QCAWLAN_TOOL_LIST+= $(call if_opt_set, ATH_SUPPORT_IBSS_PRIVATE_SECURITY, athadhoc)
QCAWLAN_TOOL_LIST+= $(call if_opt_set, ATH_SSID_STEERING, ssidsteering)
QCAWLAN_TOOL_LIST+= $(call if_opt_set, ATH_SUPPORT_TX99, tx99tool)
QCAWLAN_TOOL_LIST+= $(call if_opt_set, DEBUG_TOOLS, dumpregs reg)
QCAWLAN_TOOL_LIST+= $(call if_opt_set, WIFISTATS_TOOL_SUPPORT, wifistats)
QCAWLAN_TOOL_LIST+= $(call if_opt_set, ATH_ACS_DEBUG_SUPPORT, acsdbgtool)
QCAWLAN_TOOL_LIST+= $(call if_opt_set, QCA_CFR_SUPPORT, ../../../component_dev/tools/linux/cfr_test_app)
QCAWLAN_TOOL_LIST+= $(call if_opt_set, QCA_SUPPORT_RDK_STATS, ../../../component_dev/tools/linux/peerratestats)

