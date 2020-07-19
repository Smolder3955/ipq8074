include $(TOPDIR)/rules.mk

PKG_NAME:=mdump
PKG_VERSION:=g3d8426a
PKG_RELEASE:=0

LOCAL_SRC:=$(TOPDIR)/qca/src/nss-userspace
include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
PKG_REV:=3d8426a
PKG_VERSION:=g3d8426a
endif

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
PKG_BRANCH:=$(basename $(PKG_VERSION))

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=$(PKG_NAME)
endef

define Package/$(PKG_NAME)/Description
  System memory dump utility
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) $(LOCAL_SRC)/mdump/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	CC="$(TARGET_CC)" \
	CFLAGS="$(TARGET_CFLAGS)" \
	CPPFLAGS="$(TARGET_CPPFLAGS)" \
	LDFLAGS="$(TARGET_LDFLAGS)" \
	$(MAKE) -C $(PKG_BUILD_DIR)
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/$(PKG_NAME) $(1)/bin/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
