include $(TOPDIR)/rules.mk

PKG:=libtcmd
PKG_NAME:=$(PKG)
PKG_RELEASE:=1

LOCAL_SRC:=$(TOPDIR)/qca/src/common-tools/ath6kl-utils/libtcmd

include $(INCLUDE_DIR)/local-development.mk

ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif

#PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=QCA
  CATEGORY:=QCA Proprietary software
  URL:=http://www.qca.qualcomm.com
  MAINTAINER:=Qualcomm Atheros
  TITLE:= QCA libtcmd utils
  DEPENDS:= @TARGET_ipq_ipq807x||TARGET_ipq_ipq807x_64 +libpthread +libnl
endef

define Package/$(PKG_NAME)/description/Default
  LIBTCMD Package Support for QCA WIFI 11 drivers
endef


TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include \
		 -I$(STAGING_DIR)/usr/include/libnl3 \
		 -I$(STAGING_DIR)/usr/include/libnl3/netlink \
		 -fPIC -DWLAN_API_NL80211 -DLIBNL_2 -DWIN_AP_HOST

TARGET_LDFLAGS += -lnl-3 -lnl-genl-3 -shared

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) \
	CC="$(TARGET_CC)" \
	CFLAGS="$(TARGET_CFLAGS)" \
	LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP)	$(PKG_BUILD_DIR)/libtcmd.h	$(STAGING_DIR)/usr/include/
	$(CP)   $(PKG_BUILD_DIR)/libtcmd.so	$(STAGING_DIR)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/libtcmd.so $(1)/usr/lib/
endef

$(eval $(call BuildPackage,libtcmd))






