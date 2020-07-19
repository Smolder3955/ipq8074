include $(TOPDIR)/rules.mk
PKG:=athdiag
PKG_NAME:=$(PKG)
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=1

include $(INCLUDE_DIR)/local-development.mk
TARGET_LDFLAGS += $(if $(CONFIG_USE_EGLIBC),-lrt) -pie
TARGET_CFLAGS += -fpie

PKG_SOURCE_VERSION:=$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/athdiag
  SECTION:=QCA
  CATEGORY:=QCA Proprietary software
  TITLE:= QCA ATHDIAG(athdiag) Driver & tools
  URL:=http://www.qca.qualcomm.com
  DEPENDS:=+qca-wifi
endef

define Package/athdiag/description
  athdiag command support for ath10k
endef

define Build/Compile
	CFLAGS="$(TARGET_CFLAGS)" \
	$(MAKE) -C $(PKG_BUILD_DIR)/ \
	TOOLPREFIX="$(TARGET_CROSS)" \
	LIBS="$(TARGET_LDFLAGS)" \
	DL_DIR="$(DL_DIR)" \
	WLAN_TOP=$(PKG_BUILD_DIR) \
	all
endef

define Package/athdiag/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_DIR) $(1)/lib
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/athdiag $(1)/usr/sbin
	#$(INSTALL_DATA) $(PKG_BUILD_DIR)/testcmd_tlv/Linux/*.so $(1)/lib
endef

$(eval $(call BuildPackage,athdiag))

