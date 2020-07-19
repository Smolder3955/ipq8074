include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/package.mk

PKG_RELEASE:=1

define Package/hyfi/Default
  SECTION:=QCA
  CATEGORY:=QCA Proprietary software
  URL:=http://www.qca.qualcomm.com
  MAINTAINER:=Qualcomm Atheros
  DEPENDS:=+kmod-qca-hyfi-bridge +kmod-qca-hyfi-iptv-helper +qca-libhyfi-bridge \
	       +qca-hyctl +kmod-qca-hyfi-qdisc +qca-wsplcd +libwpa2 \
	       +libhyficommon +libstorage +libieee1905 +qca-acd +qca-wifison-ext-lib
  MAINTAINER:=Qualcomm Atheros
endef

define Package/hyfi
  $(call Package/hyfi/Default)
  TITLE:=Hy-Fi - selects all Hy-Fi components for build
  DEPENDS+=+qca-hyd
  MAINTAINER:=Qualcomm Atheros
endef

define Package/hyfi/description
  Meta package which selects all Hy-Fi components require for build
endef

define Package/hyfi/install
endef

define Package/hyfi-plc
  $(call Package/hyfi/Default)
  TITLE:=Hy-Fi - selects all Hy-Fi and PLC components for build
  DEPENDS+=+qca-hyd-plc +qca-vhyfid +qca-plc-serv
endef

define Package/hyfi-plc/description
  Meta package which selects all Hy-Fi and PLC components require for build
endef

define Package/hyfi-plc/install
endef

PKG_NAME:=hyfi-ui
define Package/$(PKG_NAME)
  SECTION:=QCA
  CATEGORY:=QCA Proprietary software
  URL:=http://www.qca.qualcomm.com
  MAINTAINER:=Qualcomm Atheros
  TITLE:=Hy-Fi - selects all Hy-Fi web interfaces
  DEPENDS:=+luci-hyfi +luci-hyfi-advanced +luci-wsplc +luci-wsplc-advanced
  MAINTAINER:=Qualcomm Atheros
endef

define Package/$(PKG_NAME)/description
  Meta package which selects all Hy-Fi web interfaces
endef

define Package/$(PKG_NAME)/install
endef

define Build/Compile
endef

$(eval $(call BuildPackage,hyfi))
$(eval $(call BuildPackage,hyfi-plc))
$(eval $(call BuildPackage,hyfi-ui))
