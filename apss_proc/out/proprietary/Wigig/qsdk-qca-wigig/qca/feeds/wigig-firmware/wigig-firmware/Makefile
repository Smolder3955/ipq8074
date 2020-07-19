include $(TOPDIR)/rules.mk

PKG_NAME:=wigig-firmware
PKG_FOLDER:=WIGIG.TLN.7.5
PKG_VERSION:=WIGIG.TLN.7.5-00071-WIGIGTLNZ-1
PKG_RELEASE:=1

PKG_SOURCE:=$(PKG_VERSION).tar.bz2
PKG_SOURCE_URL:=http://vm-cnsswebserv/cnss_win/santaclara/dev01/$(PKG_FOLDER)/$(PKG_VERSION)/
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)/Default
  SECTION:=firmware
  CATEGORY:=Firmware
  DEPENDS+= @TARGET_ipq806x||TARGET_ipq_ipq806x||TARGET_ipq_ipq807x||TARGET_ipq_ipq807x_64
  TITLE:=QCA wigig(wil6210,11ad) firmware
endef

define Package/$(PKG_NAME)/description/Default
  This package contains the wigig(wil61210,11ad) Firmware binaries
endef

Build/Compile=true

define Package/wigig-firmware/install/Default
	$(INSTALL_DIR) $(1)/lib/firmware
	$(TAR) xjvf $(TOPDIR)/dl/$(PKG_VERSION).tar.bz2 -C $(1)/lib/firmware/ --strip-components=1
endef

define Package/$(PKG_NAME)
$(call Package/$(PKG_NAME)/Default)
endef

define Package/$(PKG_NAME)/description
$(call Package/$(PKG_NAME)/description/Default)
endef

define Package/$(PKG_NAME)/install
$(call Package/$(PKG_NAME)/install/Default,$(1))
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
