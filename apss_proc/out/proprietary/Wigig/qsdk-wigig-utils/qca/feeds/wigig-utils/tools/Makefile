include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-wigig-tools
PKG_RELEASE:=1

LOCAL_SRC:=$(TOPDIR)/qca/src/wigig-utils/tools
include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_REV:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
  PKG_VERSION:=g$(PKG_REV)
endif

QCA_WIGIGTOOLS_MAKE_OPTS:= \
	CROSS=$(TARGET_CROSS) \
	WIGIGTOOLSDIR=$(PKG_BUILD_DIR) \

include $(INCLUDE_DIR)/package.mk

PKG_BUILD_DEPENDS:=qca-hostap

define Package/qca-wigig-tools
  SECTION:=QCA
  CATEGORY:=QCA Proprietary software
  URL:=http://www.qca.qualcomm.com
  MAINTAINER:=Qualcomm Atheros
  DEPENDS:=@TARGET_ipq806x||TARGET_ipq_ipq806x||TARGET_ipq_ipq807x||TARGET_ipq_ipq807x_64 +libnl +librt +qca-libwigigaoa
  TITLE:=Wigig Tools
endef

define Package/qca-wigig-tools/description
  Various tools for advanced Wigig usage
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR)/wigigrftool $(strip $(QCA_WIGIGTOOLS_MAKE_OPTS))
	$(MAKE) -C $(PKG_BUILD_DIR)/wigigaptest $(strip $(QCA_WIGIGTOOLS_MAKE_OPTS))
	$(MAKE) -C $(PKG_BUILD_DIR)/wigigftmtest $(strip $(QCA_WIGIGTOOLS_MAKE_OPTS))
endef

define Package/qca-wigig-tools/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/install/usr/sbin/wigigrftool $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/install/usr/sbin/wigigaptest $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/install/usr/sbin/wigigftmtest $(1)/usr/sbin
endef

$(eval $(call BuildPackage,qca-wigig-tools))
