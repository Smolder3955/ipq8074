###################################################
# Makefile for the NSS data plane driver
###################################################

obj ?= .

obj-m += qca-nss-dp.o

qca-nss-dp-objs += edma/edma_cfg.o \
		   edma/edma_data_plane.o \
		   edma/edma_tx_rx.o \
		   nss_dp_attach.o \
		   nss_dp_ethtools.o \
		   nss_dp_main.o \
		   nss_dp_switchdev.o

qca-nss-dp-objs += gmac_hal_ops/qcom/qcom_if.o
qca-nss-dp-objs += gmac_hal_ops/syn/syn_if.o

NSS_DP_INCLUDE = -I$(obj)/include -I$(obj)/exports -I$(obj)/gmac_hal_ops/include

ccflags-y += $(NSS_DP_INCLUDE)

ifeq ($(SoC),$(filter $(SoC),ipq807x ipq807x_64 ipq60xx ipq60xx_64))
ccflags-y += -DNSS_DP_PPE_SUPPORT
endif

ifeq ($(SoC),$(filter $(SoC),ipq60xx))
ccflags-y += -DNSS_DP_IPQ60XX
endif

ifeq ($(SoC),$(filter $(SoC),ipq807x ipq807x_64))
ccflags-y += -DNSS_DP_IPQ807X
endif
