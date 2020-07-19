
include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-cnss-daemon
PKG_SOURCE_PROTO:=git
PKG_BRANCH:=master
PKG_RELEASE:=1

include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif
PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)
PKG_SOURCE_VERSION:=$(PKG_VERSION)

include $(INCLUDE_DIR)/package.mk

define Package/qca-cnss-daemon
  SECTION:=QCA
  CATEGORY:=QCA Proprietary software
  TITLE:=QCA Linux QMI sotware
  DEPENDS:=@TARGET_ipq_ipq807x||TARGET_ipq_ipq807x_64 +libnl +libpthread +qca-qmi-framework +qca-qmi +qca-data
endef

define Package/qca-cnss-daemon/Description
  This package contains a Linux qmi framework for the user space
endef

TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include

define Build/InstallDev
	$(INSTALL_DIR) $(1)
	mkdir -p $(1)/usr/include
	$(CP) $(PKG_BUILD_DIR)/*.h $(1)/usr/include/
endef

define Package/qca-cnss-daemon/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/lib/cnssdaemon $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/lib/cnsscli $(1)/usr/bin
endef


$(eval $(call BuildPackage,qca-cnss-daemon))
