#
#  Copyright (c) 2014, 2017 Qualcomm Innovation Center, Inc.
#  All Rights Reserved.
#  Confidential and Proprietary - Qualcomm Innovation Center, Inc.
#
#
#  2014 Qualcomm Atheros, Inc.  All rights reserved. 
#
#  Qualcomm is a trademark of Qualcomm Incorporated, registered in the United
#  States and other countries.  All Qualcomm Incorporated trademarks are used with
#  permission.  Atheros is a trademark of Qualcomm Atheros, Inc., registered in
#  the United States and other countries.  Other products and brand names may be
#  trademarks or registered trademarks of their respective owners. 
#

export BUILD_ADF=0

TFTPPATH := /tftpboot/wlan

#
# Include files needed for HTC/WMI/HIF
#

ifeq ($(strip ${BUS}),USB)
BUSNAME=usb
COPTS += -DATH_USB

# tempary for usbdrv.c
INCS += -I$(obj)/$(DEPTH)/htc/hif/usb
endif

ifeq ($(strip ${BUS}),GMAC)
BUSNAME=gmac
COPTS += -DATH_USB -DATH_GMAC -DUMAC_SUPPORT_NAWDS=1 -DATH_SUPPORT_QUICK_KICKOUT=1 -DATH_SUPPORT_STATS_APONLY=1
EXTRA_CFLAGS += -DHTC_HOST_CREDIT_DIST=1 -DATH_HTC_TX_SCHED=1 -DENCAP_OFFLOAD -DATH_HTC_SG_SUPPORT=1 -DWMI_RETRY -DATH_HTC_MII_RXIN_TASKLET  -DATHHTC_AP_REMOVE_STATS -DATH_HTC_REMOVE_RXTIMER_EARLYTIMEOUTFIX 
COPTS :=$(patsubst -DATH_AMSDU=1,, $(COPTS))
endif

ifeq ($(strip ${BUS}),PCI)
BUSNAME=pci
COPTS += -DATH_USB
endif
