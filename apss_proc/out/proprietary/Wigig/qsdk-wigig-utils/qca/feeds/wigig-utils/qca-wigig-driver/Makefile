include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=qca-wigig-driver
PKG_RELEASE:=1

LOCAL_SRC=$(LINUX_DIR)/drivers/net/wireless/ath/wil6210
include $(INCLUDE_DIR)/local-development.mk
ifeq ($(DUMP)$(PKG_VERSION),)
	PKG_VERSION:=$(shell git ls-remote $(PKG_SOURCE_URL) $(PKG_BRANCH) | cut -b -7)
endif

include $(INCLUDE_DIR)/package.mk

define KernelPackage/qca-wigig-driver/config
	if PACKAGE_kmod-qca-wigig-driver
		if PACKAGE_kmod-qca-nss-drv
		config PACKAGE_QCA_WIGIG_NSS_SUPPORT
			bool "Enable NSS support in 11ad wil6210 driver"
			default y
			help
			This option enables NSS support in 11ad wil6210 driver
		endif
	endif
endef

ifeq (y, $(CONFIG_PACKAGE_QCA_WIGIG_NSS_SUPPORT))
	WIL6210_NSS_SUPPORT_FLAG:=CONFIG_WIL6210_NSS_SUPPORT=y
else
	WIL6210_NSS_SUPPORT_FLAG:=CONFIG_WIL6210_NSS_SUPPORT=n
endif

define KernelPackage/qca-wigig-driver
	SECTION:=kernel
	CATEGORY:=Kernel modules
	SUBMENU:=Wireless Drivers
	DEPENDS+=@TARGET_ipq806x||TARGET_ipq_ipq806x||TARGET_ipq_ipq807x||TARGET_ipq_ipq807x_64 @PCI_SUPPORT +kmod-cfg80211-linux +wigig-mac80211 +kmod-qca-nss-drv
	KCONFIG+=CONFIG_ATH_CARDS=m \
		CONFIG_ATH5K_PCI=n \
		CONFIG_WIL6210=m \
		CONFIG_WIL6210_ISR_COR=y \
		$(WIL6210_NSS_SUPPORT_FLAG) \
		CONFIG_WIL6210_TRACING=n \
		CONFIG_WIL6210_WRITE_IOCTL=y \
		CONFIG_WIL6210_DEBUGFS=y \
		CONFIG_MWIFIEX=n \
		CONFIG_WILC1000_DRIVER=n
	TITLE:=QCA 802.11ad wireless cards support
	FILES:=$(PKG_BUILD_DIR)/wil6210.ko
	AUTOLOAD:=$(call AutoProbe,wil6210)
endef

define KernelPackage/qca-wigig-driver/Description
This module adds support for wireless adapters based on
802.11ad (60GHz) QCA family of chipsets.
endef

define Build/Compile
	$(MAKE) -C "$(LINUX_DIR)" \
		CROSS_COMPILE="$(TARGET_CROSS)" \
		ARCH="$(LINUX_KARCH)" \
		EXTRA_CFLAGS="-I$(PKG_BUILD_DIR)/include -I$(STAGING_DIR)/usr/include/qca-nss-drv" \
		$(WIL6210_NSS_SUPPORT_FLAG) \
		SUBDIRS="$(PKG_BUILD_DIR)" \
		modules
endef

$(eval $(call KernelPackage,qca-wigig-driver))
