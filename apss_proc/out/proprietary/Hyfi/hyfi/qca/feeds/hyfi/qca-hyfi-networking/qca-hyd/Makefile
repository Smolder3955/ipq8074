include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-hyd
PKG_RELEASE:=1

include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
PKG_REV:=d9fff89
PKG_VERSION:=gd9fff89
endif
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

WIFI_U:=PACKAGE_kmod-qca-wifi-unified-profile
WIFI_A:=PACKAGE_kmod-qca-wifi-akronite-perf
WIFI_D:=PACKAGE_kmod-qca-wifi-dakota-perf
WIFI_MIPS:=PACKAGE_kmod-qca-wifi-perf
WIFI_MIPS_UNIFIED:=PACKAGE_kmod-qca-wifi-unified-perf
WIFI_10_2_MIPS:=PACKAGE_kmod-qca-wifi-perf
WIFI_L:=PACKAGE_kmod-qca-wifi-lowmem-profile
QCA_PLC:=PACKAGE_qca-plc-serv
WIFI_10_2_ARM:=PACKAGE_kmod-qca-wifi-akronite-perf
WIFI_PKGS:=$(WIFI_U)||$(WIFI_A)||$(WIFI_D)||$(WIFI_MIPS)||$(WIFI_MIPS_UNIFIED)||$(WIFI_10_2_MIPS)||$(WIFI_10_2_ARM)||$(WIFI_L)
PKG_BUILD_DEPENDS:=$(WIFI_U):qca-wifi $(WIFI_A):qca-wifi $(WIFI_D):qca-wifi $(WIFI_MIPS):qca-wifi $(WIFI_L):qca-wifi \
	$(WIFI_MIPS_UNIFIED):qca-wifi $(WIFI_10_2_MIPS):qca-wifi $(WIFI_10_2_ARM):qca-wifi +qca-ssdk $(QCA_PLC):qca-plc-serv

TARGET_CFLAGS += \
        -I$(STAGING_DIR)/usr/include/plcserv/

TARGET_CFLAGS += -fpie
TARGET_LDFLAGS += -pie

QCAHYD_MAKE_OPTS:= \
        CROSS=$(TARGET_CROSS) \
        GWLIB=$(STAGING_DIR)/usr/lib \
        HYTOP=$(PKG_BUILD_DIR) \
        GWINCLUDE=$(STAGING_DIR)/usr/include/hyficommon \
        ATHDIR=$(STAGING_DIR)/usr/include \
        PLCSRV=$(STAGING_DIR)/usr/include/plcserv \
        HYFIDIR=$(STAGING_DIR)/usr/include/hyfibr \
        HYFIMCDIR=$(STAGING_DIR)/usr/include/hyfibr \
        HYFIBRLIBDIR=$(STAGING_DIR)/usr/include/libhyfibr \
        IEEE1905DIR=$(STAGING_DIR)/usr/include/ieee1905 \
        QCASSDKDIR=$(STAGING_DIR)/usr/include/qca-ssdk \
        QCACFLAGS="$(TARGET_CFLAGS)" \
        QCALDFLAGS="-Wl,--gc-sections $(TARGET_LDFLAGS)" \
        INSTALL_ROOT=$(PKG_BUILD_DIR)/install \
        KERNELVERSION=$(LINUX_VERSION) \
        STAGING_DIR=$(STAGING_DIR)

ifneq ($(CONFIG_PACKAGE_qca-wifison-ext-lib),)
QCAHYD_MAKE_OPTS+=SONEVENTINCLUDE=$(STAGING_DIR)/usr/include/qca-wifison-ext-lib
QCAHYD_MAKE_OPTS+=HYD_MODULE_SONEVENT=y
endif

ifneq ($(CONFIG_HYD_SUPPORT_CFG80211),)
QCAHYD_MAKE_OPTS += LIBCFG80211_SUPPORT=y
endif

ifneq ($(CONFIG_HYD_GIGABIT_ETHERNET),)
QCAHYD_MAKE_OPTS+=HYBRID_ETH_SPEED=1000
endif

ifneq ($(CONFIG_PACKAGE_qca-plc-serv),)
QCAHYD_MAKE_OPTS+=HYD_MODULE_PLC=y
endif

ifneq ($(CONFIG_HYD_FAST_ETHERNET),)
QCAHYD_MAKE_OPTS+=HYBRID_ETH_SPEED=100
endif

ifeq ($(CONFIG_PACKAGE_qca-vhyfid),y)
QCAHYD_MAKE_OPTS+=GATEWAY_PROFILE_VHYFI=y
endif

ifeq ($(CONFIG_HYD_WIFI_LOAD_BALANCING),y)
QCAHYD_MAKE_OPTS+=WHCWLBDIR=$(STAGING_DIR)/usr/include/whc-wlb
QCAHYD_MAKE_OPTS+=HYD_MODULE_WLB=y
endif

ifeq ($(CONFIG_HYD_SUPPORT_VHT160),y)
QCAHYD_MAKE_OPTS+=HYD_SUPPORT_VHT160=y
endif

ifeq ($(CONFIG_HYD_SUPPORT_SONLIB),y)
QCAHYD_MAKE_OPTS+=HYD_SUPPORT_SONLIB=y
endif

__busybox_have_default:=$(shell $(SH_FUNC) grep '^CONFIG_BUSYBOX_DEFAULT_HAVE_DOT_CONFIG' $(TOPDIR)/.config)
ifeq ($(__busybox_have_default),)
BUSYBOX_SYM:=CONFIG
else
BUSYBOX_SYM:=DEFAULT
endif

LOCAL_VARIANT=$(patsubst qca-hyd-%,%,$(patsubst qca-hyd-%,%,$(BUILD_VARIANT)))

define Package/qca-hyd/config
  source "$(SOURCE)/Config.in"
endef

define Package/qca-hyd
  SECTION:=QCA
  CATEGORY:=QCA Proprietary software
  URL:=http://www.qca.qualcomm.com
  MAINTAINER:=Qualcomm Atheros
  DEPENDS:=@$(WIFI_PKGS) +qca-ieee1905-init +kmod-qca-hyfi-bridge +qca-libhyfi-bridge +libieee1905 \
	   +qca-hyctl +libhyficommon +qca-wifison-ext-lib +libwifisoncfg +libpthread +@BUSYBOX_$(BUSYBOX_SYM)_STAT \
	   +@BUSYBOX_$(BUSYBOX_SYM)_FEATURE_STAT_FORMAT +HYD_WIFI_LOAD_BALANCING:qca-whc-lbd \
	   +jansson
  TITLE:=Hy-Fi Path-Selection daemon
  SUBMENU:=Hy-Fi features
endef

define Package/qca-hyd/description
  This package installs the Hy-Fi Path-Selection daemon
endef

#define Package/qca-hyd-plc/config
#  $(call Package/qca-hyd/config)
#endef

define Package/qca-hyd-plc
  $(call Package/qca-hyd)
  DEPENDS+=+qca-plc-serv
  VARIANT:=plc
endef

define Package/qca-wifison-ext-lib
  $(call Package/qca-hyd)
  DEPENDS+=+qca-wifison-ext-lib
endef

define Package/qca-hyd-plc/description
  This package installs the Hy-Fi Path-Selection daemon build with PLC
  support.
endef

QCA_HYD_HEADERS= \
                $(PKG_BUILD_DIR)/include/heServiceConfig.h   \
                $(PKG_BUILD_DIR)/include/logServiceConfig.h  \
                $(PKG_BUILD_DIR)/include/pcpServiceConfig.h  \
                $(PKG_BUILD_DIR)/include/pcwServiceConfig.h  \
                $(PKG_BUILD_DIR)/include/psServiceConfig.h   \
                $(PKG_BUILD_DIR)/include/tdServiceConfig.h   \
                $(PKG_BUILD_DIR)/include/wlanManagerConfig.h \
                $(PKG_BUILD_DIR)/include/hcpConfig.h \
                $(PKG_BUILD_DIR)/include/ieee1905Config.h

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include/hyd
	$(foreach header_file,$(QCA_HYD_HEADERS), $(CP) $(header_file) $(1)/usr/include/hyd;)
endef

ifeq ($(LOCAL_VARIANT),plc)
QCAHYD_MAKE_OPTS+=HYD_MODULE_PLC=y
endif

define Build/Compile
	mkdir -p $(PKG_BUILD_DIR)/install/lib
	mkdir -p $(PKG_BUILD_DIR)/install/include
	mkdir -p $(PKG_BUILD_DIR)/install/sbin
	$(MAKE) -C $(PKG_BUILD_DIR) $(strip $(QCAHYD_MAKE_OPTS)) subdirs
endef

INSTALL_LIBS:= libpluginManager libpsService libmcfwdtblwlan5g libmcfwdtblwlan2g libmcfwdtbleswitch

define Package/qca-hyd/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_DIR) $(1)/usr/lib/
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_DIR) $(1)/etc/init.d
	$(if $(INSTALL_LIBS),$(CP) $(foreach lib,$(INSTALL_LIBS),$(PKG_BUILD_DIR)/install/lib/$(lib).so) $(1)/usr/lib)
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/install/sbin/hyd $(1)/usr/sbin
	$(INSTALL_BIN) ./files/hyt $(1)/usr/sbin/hyt
	$(INSTALL_CONF) ./files/hyd.config $(1)/etc/config/hyd
ifeq ($(LINUX_VERSION),3.3.8)
	$(INSTALL_BIN) ./files/hyd_noprocd.init $(1)/etc/init.d/hyd
else
	$(INSTALL_BIN) ./files/hyd.init $(1)/etc/init.d/hyd
endif
	$(INSTALL_DIR) $(1)/etc/hotplug.d/net
	$(INSTALL_BIN) ./files/hyd.hotplug $(1)/etc/hotplug.d/net/30-hyd
endef

define Package/qca-hyd-plc/install
	$(call Package/qca-hyd/install, $(1))
endef

$(eval $(call BuildPackage,qca-hyd))
$(eval $(call BuildPackage,qca-hyd-plc))
