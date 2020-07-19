ifeq ($(obj),)
obj := .
endif

DEPTH := ..

#
# Path to the QTI driver framework source base
#
QDF := $(src)/../cmn_dev/qdf

#
# Path to the Atheros service framework source base
#
ASF := $(src)

ifeq ($(strip ${QCA_PARTNER_MAKE_F_SUPPORT}),1)
export QCA_PARTNER_MAKE_F_INC=1
endif
include $(obj)/$(DEPTH)/os/linux/Makefile-linux.common

INCS += -I$(ASF)/include -I$(QDF)/inc -I$(ASF)/../pld/inc
obj-m += smart_antenna.o

EXTRA_CFLAGS+= $(INCS) $(COPTS)
