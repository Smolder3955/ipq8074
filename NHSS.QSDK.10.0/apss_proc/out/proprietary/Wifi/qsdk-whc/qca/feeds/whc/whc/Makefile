include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/package.mk

# The packages defined in this file are just "meta" packages that pull in
# other ones.
PKG_RELEASE:=1

PKG_NAME:=whc
define Package/$(PKG_NAME)
  SECTION:=QCA
  CATEGORY:=QCA Proprietary software
  URL:=http://www.qca.qualcomm.com
  MAINTAINER:=Qualcomm Atheros
  TITLE:=Whole Home Coverage - selects all Whole Home Coverage components for build
  DEPENDS:=+qca-whc-lbd +qca-whc-repacd
  MAINTAINER:=Qualcomm Atheros
endef

define Package/$(PKG_NAME)/description
  Meta package which selects all Whole Home Coverage components required for build
endef

define Package/$(PKG_NAME)/install
endef

PKG_NAME:=whc-ui
define Package/$(PKG_NAME)
  SECTION:=QCA
  CATEGORY:=QCA Proprietary software
  URL:=http://www.qca.qualcomm.com
  MAINTAINER:=Qualcomm Atheros
  TITLE:=Whole Home Coverage - selects all Whole Home Coverage web interface components
  DEPENDS:=+luci-whc-lbd +luci-whc-lbd-advanced +luci-whc-lbd-diaglog \
           +luci-whc-repacd +luci-whc-repacd-advanced
  MAINTAINER:=Qualcomm Atheros
endef

define Package/$(PKG_NAME)/description
  Meta package which selects all Whole Home Coverage web interface components
endef

define Package/$(PKG_NAME)/install
endef

define Build/Compile
endef

$(eval $(call BuildPackage,whc))
$(eval $(call BuildPackage,whc-ui))
