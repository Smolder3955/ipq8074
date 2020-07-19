include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-libwigigaoa
PKG_RELEASE:=1

LOCAL_SRC:=$(TOPDIR)/qca/src/wigig-utils-noship/location/libaoa
include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_REV:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
  PKG_VERSION:=g$(PKG_REV)
endif

QCA_LIBWIGIGAOA_MAKE_OPTS:= \
	CROSS=$(TARGET_CROSS) \
	LIBAOA_DIR=$(PKG_BUILD_DIR) \

include $(INCLUDE_DIR)/package.mk

PKG_BUILD_DEPENDS:=qca-hostap

define Package/qca-libwigigaoa
  SECTION:=QCA
  CATEGORY:=QCA Proprietary software
  URL:=http://www.qca.qualcomm.com
  MAINTAINER:=Qualcomm Atheros
  DEPENDS:=@TARGET_ipq806x||TARGET_ipq_ipq806x||TARGET_ipq_ipq807x||TARGET_ipq_ipq807x_64 +librt
  TITLE:=Wigig AOA Algorithm Library
endef

define Package/qca-libwigigaoa/description
  Library implementing WIGIG AOA azimuth/elevation measurement algorithm
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) $(strip $(QCA_LIBWIGIGAOA_MAKE_OPTS))
endef

PKG_INSTALL_DIR := $(PKG_BUILD_DIR)/install
INSTALL_LIBS := libwigigaoa
INSTALL_HEADERS := aoa_api.h
INSTALL_CLDB := aoa_cldb_falcon.bin aoa_cldb_swl14.bin

define Package/qca-libwigigaoa/install
	$(INSTALL_DIR) $(1)/usr/lib/
	$(INSTALL_DIR) $(1)/etc/wigig/
	$(if $(INSTALL_LIBS),$(CP) $(foreach lib,$(INSTALL_LIBS),$(PKG_INSTALL_DIR)/lib/$(lib).so) $(1)/usr/lib)
	$(if $(INSTALL_CLDB),$(CP) $(foreach cldb,$(INSTALL_CLDB),$(PKG_INSTALL_DIR)/etc/wigig/$(cldb)) $(1)/etc/wigig)
endef

define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DIR) $(1)/usr/include
	$(INSTALL_DIR) $(1)/etc/wigig
	$(if $(INSTALL_LIBS),$(CP) $(foreach lib,$(INSTALL_LIBS),$(PKG_INSTALL_DIR)/lib/$(lib).so) $(1)/usr/lib)
	$(if $(INSTALL_HEADERS),$(CP) $(foreach header,$(INSTALL_HEADERS),$(PKG_INSTALL_DIR)/include/$(header)) $(1)/usr/include)
	$(if $(INSTALL_CLDB),$(CP) $(foreach cldb,$(INSTALL_CLDB),$(PKG_INSTALL_DIR)/etc/wigig/$(cldb)) $(1)/etc/wigig)
endef

$(eval $(call BuildPackage,qca-libwigigaoa))
