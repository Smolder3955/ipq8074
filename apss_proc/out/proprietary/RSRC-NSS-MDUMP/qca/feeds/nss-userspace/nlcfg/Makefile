include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=nlcfg
#PKG_VERSION:=1.0
PKG_RELEASE:=1
NLCFG_BUILD_DIR:=$(TOPDIR)/qca/src/nss-userspace

include $(INCLUDE_DIR)/package.mk

define Package/nlcfg
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Userspace utility for pushing rule into NSS
  DEPENDS:=+libpthread +libnl +kmod-qca-nss-drv-netlink
endef

define Package/nlcfg/description
 A userspace utility for pushing rule into NSS.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)/nlcfg
	mkdir -p $(PKG_BUILD_DIR)/lib
	$(CP) $(NLCFG_BUILD_DIR)/nlcfg/*  $(PKG_BUILD_DIR)/nlcfg/
	$(CP) $(NLCFG_BUILD_DIR)/lib/*  $(PKG_BUILD_DIR)/lib/
endef

TOOL_CFLAGS:= -I$(STAGING_DIR)/usr/include/qca-nss-clients \
	      -I$(STAGING_DIR)/usr/include/qca-nss-drv \
	      -I$(STAGING_DIR)/usr/include/qca-nss-crypto \
	      -I$(STAGING_DIR)/usr/include/libnl3 \
	      -I$(STAGING_DIR)/usr/include/nlcfg/

TOOL_LDFLAGS:= -L$(STAGING_DIR)/lib

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR)/lib \
		CC="$(TARGET_CC)" \
		AR="$(TARGET_AR) " \
		CFLAGS="$(TOOL_CFLAGS)" \
		LD_LIBRARY_PATH="$(TOOL_LDFLAGS)"

	$(MAKE) -C $(PKG_BUILD_DIR)/nlcfg \
		CC="$(TARGET_CC)" \
		CFLAGS="$(TOOL_CFLAGS)" \
		LD_LIBRARY_PATH="$(TOOL_LDFLAGS)"
endef

define Package/nlcfg/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_DIR) $(1)/lib
	$(INSTALL_DATA) $(PKG_BUILD_DIR)/lib/obj/libnl-nss.so $(1)/lib/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/nlcfg/obj/nlcfg $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,nlcfg))

