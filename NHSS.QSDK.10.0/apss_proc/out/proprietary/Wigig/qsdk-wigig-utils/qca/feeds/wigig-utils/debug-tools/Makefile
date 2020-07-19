include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-wigig-debug-tools
PKG_RELEASE:=1

LOCAL_SRC:=$(TOPDIR)/qca/src/wigig-utils/debug-tools
include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_REV:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
  PKG_VERSION:=g$(PKG_REV)
endif

QCA_DEBUG_TOOLS_MAKE_OPTS:= \
	CROSS=$(TARGET_CROSS) \
	DEBUG_TOOLS_DIR=$(PKG_BUILD_DIR) \

include $(INCLUDE_DIR)/package.mk

define Package/qca-wigig-debug-tools
  SECTION:=QCA
  CATEGORY:=QCA Proprietary software
  URL:=http://www.qca.qualcomm.com
  MAINTAINER:=Qualcomm Atheros
  DEPENDS:=@TARGET_ipq806x||TARGET_ipq_ipq806x||TARGET_ipq_ipq807x||TARGET_ipq_ipq807x_64 +libstdcpp +libpthread +libnl +librt
  TITLE:=Qualcomm Atheros 802.11ad Debug Tools
endef

define Package/qca-wigig-debug-tools/description
  This package contains debug tools for Qualcomm Atheros 802.11ad device
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) $(strip $(QCA_DEBUG_TOOLS_MAKE_OPTS))
endef

define Package/qca-wigig-debug-tools/install
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/lib/utils/libwigig_utils.so $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/lib/FlashAcss/libwigig_flashaccess.so $(1)/usr/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/lib/WlctPciAcss/libwigig_pciaccess.so $(1)/usr/lib

	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/host_manager_11ad/host_manager_11ad $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/shell_11ad/shell_11ad $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/wiburn/wigig_wiburn $(1)/usr/sbin
endef

$(eval $(call BuildPackage,qca-wigig-debug-tools))
