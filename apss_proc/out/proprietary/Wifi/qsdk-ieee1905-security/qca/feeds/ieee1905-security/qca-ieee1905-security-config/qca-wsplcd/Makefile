include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-wsplcd
PKG_RELEASE:=1

include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_REV:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
  PKG_VERSION:=g$(PKG_REV)
endif
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

TARGET_CFLAGS += -fpie
TARGET_LDFLAGS += -pie

WIFI_U:=PACKAGE_kmod-qca-wifi-unified-profile
WIFI_A:=PACKAGE_kmod-qca-wifi-akronite-perf
WIFI_D:=PACKAGE_kmod-qca-wifi-dakota-perf
WIFI_MIPS:=PACKAGE_kmod-qca-wifi-perf
WIFI_MIPS_UNIFIED:=PACKAGE_kmod-qca-wifi-unified-perf
WIFI_10_2_MIPS:=PACKAGE_kmod-qca-wifi-perf
WIFI_10_2_ARM:=PACKAGE_kmod-qca-wifi-akronite-perf
WIFI_L:=PACKAGE_kmod-qca-wifi-lowmem-profile
WIFI_PKGS:=$(WIFI_U)||$(WIFI_A)||$(WIFI_D)||$(WIFI_MIPS)||$(WIFI_MIPS_UNIFIED)||$(WIFI_10_2_MIPS)||$(WIFI_10_2_ARM)||$(WIFI_L)
PKG_BUILD_DEPENDS:=$(WIFI_U):qca-wifi $(WIFI_A):qca-wifi $(WIFI_D):qca-wifi $(WIFI_MIPS):qca-wifi \
	$(WIFI_MIPS_UNIFIED):qca-wifi $(WIFI_10_2_MIPS):qca-wifi $(WIFI_10_2_ARM):qca-wifi $(WIFI_L):qca-wifi

QCAWSPLCD_MAKE_OPTS:= \
        CROSS=$(TARGET_CROSS) \
        GWLIB=$(STAGING_DIR)/usr/lib \
        GWINCLUDE=$(STAGING_DIR)/usr/include/hyficommon \
        WPA2INCLUDE=$(STAGING_DIR)/usr/include/wpa2 \
        HYDINCLUDE=$(STAGING_DIR)/usr/include/hyd \
        STORAGEINCLUE=$(STAGING_DIR)/usr/include/storage \
        IEEE1905INCLUDE=$(STAGING_DIR)/usr/include/ieee1905 \
        PLCSRV=$(STAGING_DIR)/usr/include/plcserv \
        ATHDIR=$(STAGING_DIR)/usr/include \
        INSTALL_ROOT=$(PKG_BUILD_DIR)/install \
	EXTRA_CFLAGS="$(TARGET_CFLAGS) -Wno-error" \
	EXTRA_LDFLAGS="$(TARGET_LDFLAGS)" \
        LIBSTORAGE_UCI=y

QCAWSPLCD_DEPENDS:=@$(WIFI_PKGS) +qca-ieee1905-init +libhyficommon +libstorage +libwpa2 +libieee1905 +libuci +libpthread +libwifisoncfg
ifeq ($(CONFIG_WSPLCD_SUPPORT_PLC),y)
QCAWSPLCD_MAKE_OPTS+=WSPLCD_SUPPORT_PLC=y
QCAWSPLCD_DEPENDS+=+qca-plc-serv
endif

ifeq ($(CONFIG_WSPLCD_SUPPORT_CFG80211),y)
QCAWSPLCD_MAKE_OPTS+=WSPLCD_SUPPORT_CFG80211=y
endif

define Package/qca-wsplcd/config
  source "$(SOURCE)/Config.in"
endef

define Package/qca-wsplcd
  SECTION:=QCA
  CATEGORY:=QCA Proprietary software
  URL:=http://www.qca.qualcomm.com
  MAINTAINER:=Qualcomm Atheros, Inc.
  DEPENDS:=$(QCAWSPLCD_DEPENDS)
  TITLE:=IEEE 1905.1 AP Auto-Configuration daemon
  SUBMENU:=IEEE1905.1 features
endef

define Package/qca-wsplcd/description
  This package installs the IEEE 1905.1 AP Auto-Configuration daemon: Wi-Fi credential cloning over backbone network (e.g. HomePlugAV, Ethernet, Wi-Fi)
endef

define Build/Compile
	mkdir -p $(PKG_BUILD_DIR)/install/etc
	mkdir -p $(PKG_BUILD_DIR)/install/sbin
	$(MAKE) -C $(PKG_BUILD_DIR) $(strip $(QCAWSPLCD_MAKE_OPTS)) install
endef

define Package/qca-wsplcd/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_DIR) $(1)/etc
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/install/sbin/wsplcd $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/install/sbin/ucpkgen $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/install/etc/athx100.conf $(1)/etc
	$(INSTALL_DIR) $(1)/etc/init.d
ifeq ($(LINUX_VERSION),3.3.8)
	$(INSTALL_BIN) ./files/wsplcd_no_procd.init $(1)/etc/init.d/wsplcd
else
	$(INSTALL_BIN) ./files/wsplcd.init $(1)/etc/init.d/wsplcd
endif

	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_CONF) ./files/wsplcd.config $(1)/etc/config/wsplcd
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) ./files/uci2cfg.sh $(1)/sbin/uci2cfg.sh
	$(INSTALL_DIR) $(1)/etc/hotplug.d/button
	$(INSTALL_BIN) ./files/wsplcd.button $(1)/etc/hotplug.d/button/40-wsplcd

	$(INSTALL_DIR) $(1)/etc/wsplcd/map/templates
	$(INSTALL_CONF) ./files/scheme-*.conf $(1)/etc/wsplcd/map/templates
endef

$(eval $(call BuildPackage,qca-wsplcd))
