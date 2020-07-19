include $(TOPDIR)/rules.mk

PKG:=ftm
PKG_NAME:=$(PKG)
PKG_RELEASE:=1

LOCAL_SRC:=$(TOPDIR)/qca/src/common-tools/ftm

include $(INCLUDE_DIR)/local-development.mk

ifeq ($(DUMP)$(PKG_VERSION),)
  PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif

#PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=QCA
  CATEGORY:=QCA Proprietary software
  URL:=http://www.qca.qualcomm.com
  MAINTAINER:=Qualcomm Atheros
  TITLE:= QCA ftm utils
  DEPENDS:= @TARGET_ipq_ipq807x||TARGET_ipq_ipq807x_64 +libnl +libtcmd +qca-diag +librt +qca-IOT
endef

define Package/$(PKG_NAME)/description/Default
	FTM Package Support for QCA WIFI 11 drivers
endef

TARGET_CFLAGS += -DCONFIG_FTM_WLAN -DDEBUG -DFTM_DEBUG -DWIN_AP_HOST -DCONFIG_DAEMON_MODE
TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include/qca-diag
TARGET_CFLAGS += -MMD -O2 -Wall -g
TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include
TARGET_CFLAGS += -fpie
TARGET_LDFLAGS += -ldiag -lnl-3 -lnl-genl-3 -lrt -ltcmd
TARGET_CSRCS := ftm_main.c ftm_wlan.c ftm_write_to_flash.c
TARGET_LDFLAGS += -pie
TARGET_CFLAGS += -DIPQ_AP_HOST_IOT -ggdb3
TARGET_CSRCS += ftm_iot.c
TARGET_LDFLAGS += -lpthread
TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include/qca-iot
TARGET_LDFLAGS += -ldiag_demo

define Build/Compile
$(MAKE) -C $(PKG_BUILD_DIR) \
	CC="$(TARGET_CC)" \
	CFLAGS="$(TARGET_CFLAGS)" \
	LDFLAGS="$(TARGET_LDFLAGS)" \
	CSRCS="$(TARGET_CSRCS)"
endef

define Package/$(PKG_NAME)/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/ftm $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/ftm.init $(1)/etc/init.d/ftm
endef

$(eval $(call BuildPackage,ftm))
