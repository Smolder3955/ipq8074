#
# Copyright (c) 2016 Qualcomm Atheros, Inc..
#
# All Rights Reserved.
# Qualcomm Atheros Confidential and Proprietary.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-fst-manager
PKG_RELEASE:=1

LOCAL_SRC:=$(TOPDIR)/qca/src/wigig-utils/fst-manager
include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_REV:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
  PKG_VERSION:=g$(PKG_REV)
endif

QCA_FSTMAN_MAKE_OPTS:= \
	CROSS=$(TARGET_CROSS) \
	FSTMANDIR=$(PKG_BUILD_DIR) \

include $(INCLUDE_DIR)/package.mk

define Package/qca-fst-manager
  SECTION:=QCA
  CATEGORY:=QCA Proprietary software
  URL:=http://www.qca.qualcomm.com
  MAINTAINER:=Qualcomm Atheros
  DEPENDS:=@TARGET_ipq806x||TARGET_ipq_ipq806x||TARGET_ipq_ipq807x||TARGET_ipq_ipq807x_64 +libnl +librt
  TITLE:=FST Manager daemon
endef

define Package/qca-fst-manager/description
  This package is FST Manager daemon.
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) $(strip $(QCA_FSTMAN_MAKE_OPTS))
endef

define Package/qca-fst-manager/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/install/usr/sbin/fstman $(1)/usr/sbin
	$(INSTALL_BIN) ./files/fst.sh $(1)/usr/sbin
	$(INSTALL_DIR) $(1)/etc/config
	$(INSTALL_BIN) ./files/fst.conf $(1)/etc/config/fst
endef

$(eval $(call BuildPackage,qca-fst-manager))
