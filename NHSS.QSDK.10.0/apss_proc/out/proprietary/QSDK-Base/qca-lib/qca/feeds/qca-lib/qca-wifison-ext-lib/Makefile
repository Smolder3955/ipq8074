include $(TOPDIR)/rules.mk

PKG_NAME:=qca-wifison-ext-lib
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

define Package/qca-wifison-ext-lib
  SECTION:=QCA
  CATEGORY:=QCA Proprietary software
  URL:=http://www.qca.qualcomm.com
  MAINTAINER:=Qualcomm Atheros, Inc.
  TITLE:=Wifison Event Functionality Library
  DEPENDS:=+libnl
  SUBMENU:=Hy-Fi features
endef

define Package/qca-wifison-ext-lib/description
  This Package contains the Wifison Event Functionality Library
endef

define Package/qca-wifison-ext-lib/install
	$(INSTALL_DIR) $(1)/usr/lib/
	$(INSTALL_DIR) $(1)/usr/sbin
	$(CP) $(PKG_BUILD_DIR)/install/lib/libqca_wifison_ext.so $(1)/usr/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/install/sbin/qca_event_sample $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/install/sbin/qca_listen_port $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/install/sbin/qca_genlevent_sample $(1)/usr/sbin
endef

WIFSONEXTLIB_MAKEOPTS:= \
	TOOLPREFIX=$(TARGET_CROSS) \
	GWINCLUDE=$(STAGING_DIR)/usr/include/qca/ \
	QCACFLAGS="$(TARGET_CFLAGS)"\
	QCALDFLAGS="-Wl,--gc-sections $(TARGET_LDFLAGS)"\
	EXTRA_CFLAGS=$(STAGING_DIR)/usr/include/libnl \
	EXTRA_LDFLAGS="-lnl-3 -lnl-genl-3"\
	INSTALL_ROOT=$(PKG_BUILD_DIR)/install

define Build/Compile
	mkdir -p $(PKG_BUILD_DIR)/install
	mkdir -p $(PKG_BUILD_DIR)/install/lib/
	mkdir -p $(PKG_BUILD_DIR)/install/include/
	mkdir -p $(PKG_BUILD_DIR)/install/sbin
	$(MAKE) -C $(PKG_BUILD_DIR)/ $(strip $(WIFSONEXTLIB_MAKEOPTS))
	$(CP) $(PKG_BUILD_DIR)/libqca_wifison_ext.so  $(STAGING_DIR)/usr/lib/
	$(MAKE) -C $(PKG_BUILD_DIR)/samplecode $(strip $(WIFSONEXTLIB_MAKEOPTS))
endef

INSTALL_LIBS:= libqca_wifison_ext
INSTALL_HEADERS=wifison_event.h

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DIR) $(1)/usr/include/qca-wifison-ext-lib
	$(if $(INSTALL_LIBS),$(CP) $(foreach lib,$(INSTALL_LIBS),$(PKG_BUILD_DIR)/install/lib/$(lib).so) $(1)/usr/lib)
	$(if $(INSTALL_HEADERS),$(CP) $(foreach header,$(INSTALL_HEADERS),$(PKG_BUILD_DIR)/install/include/$(header)) $(1)/usr/include/qca-wifison-ext-lib)
endef

$(eval $(call BuildPackage,qca-wifison-ext-lib))
