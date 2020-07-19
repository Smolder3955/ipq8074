CC ?= $(TOOLPREFIX)gcc

CFLAGS += -MMD -O2 -Wall -Werror -g

ifeq ($(ARCH),mips)
CFLAGS+= -minterlink-mips16
endif

export LIBDIR ?= /lib/
export BINDIR ?= /sbin/

ifndef QSDK_WRAPD

WPA_PATH = ../athr-hostap

CFLAGS += -I$(WPA_PATH)/src
CFLAGS += -I$(WPA_PATH)/src/utils
CFLAGS += -I$(WPA_PATH)/src/drivers
CFLAGS += -I$(WPA_PATH)/src/common

endif

CFLAGS += \
	-I$(QSDK_WRAPD) \
	-I$(QSDK_WRAPD)/libxml2 \
	-I$(QSDK_WRAPD)/libnl3 \
	-I$(QSDK_WRAPD)/libnl \
	-D_GNU_SOURCE=1 -D__USE_GNU=1 -D__WIN__=1

ifdef QSDK_WRAPD
LDFLAGS += \
	-lnl-3 -lnl-genl-3
endif

ALL=wrapd

all: $(ALL)

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	for i in $(ALL); do cp $$i $(DESTDIR)$(BINDIR)/$$i; done

ifndef QSDK_WRAPD

OBJS_c = main.o wrapd_api.o $(WPA_PATH)/src/common/wpa_ctrl.o
OBJS_c += $(WPA_PATH)/src/drivers/netlink.o
OBJS_c += $(WPA_PATH)/src/utils/eloop.o
OBJS_c += $(WPA_PATH)/src/utils/wpa_debug.o
OBJS_c += $(WPA_PATH)/src/utils/common.o
OBJS_c += bridge.o
OBJS_c += wrapd_br.o
OBJS_c += cfg80211_ioctl.o

-include $(WPA_PATH)/wpa_supplicant/.config

else

OBJS_c = main.o wrapd_api.o $(WPA_PATH)/wpa_ctrl.o
OBJS_c += $(WPA_PATH)/netlink.o
OBJS_c += $(WPA_PATH)/eloop.o
OBJS_c += $(WPA_PATH)/wpa_debug.o
OBJS_c += $(WPA_PATH)/common.o
OBJS_c += bridge.o
OBJS_c += wrapd_br.o
OBJS_c += cfg80211_nl.o
OBJS_c += cfg80211_ioctl.o

endif

ifndef CONFIG_OS
CONFIG_OS=unix
endif

ifeq ($(CONFIG_OS), internal)
CFLAGS += -DOS_NO_C_LIB_DEFINES
endif

ifndef QSDK_WRAPD
OBJS_c += $(WPA_PATH)/src/utils/os_$(CONFIG_OS).o
else
OBJS_c += $(WPA_PATH)/os_$(CONFIG_OS).o
endif

ifdef CONFIG_WPA_TRACE
CFLAGS += -DWPA_TRACE


ifndef QSDK_WRAPD
OBJS_c += $(WPA_PATH)/src/utils/trace.o
OBJS_c += $(WPA_PATH)/src/utils/wpa_debug.o
else
OBJS_c += $(WPA_PATH)/trace.o
OBJS_c += $(WPA_PATH)/wpa_debug.o
endif

LDFLAGS += -rdynamic
CFLAGS += -funwind-tables
ifdef CONFIG_WPA_TRACE_BFD
CFLAGS += -DWPA_TRACE_BFD
LIBS_c += -lbfd
endif
endif

ifndef CONFIG_L2_PACKET
CONFIG_L2_PACKET=linux
endif

ifdef CONFIG_CTRL_IFACE
ifeq ($(CONFIG_CTRL_IFACE), y)
ifdef CONFIG_NATIVE_WINDOWS
CONFIG_CTRL_IFACE=named_pipe
else
CONFIG_CTRL_IFACE=unix
endif
endif
CFLAGS += -DCONFIG_CTRL_IFACE
ifeq ($(CONFIG_CTRL_IFACE), unix)
CFLAGS += -DCONFIG_CTRL_IFACE_UNIX
endif
ifeq ($(CONFIG_CTRL_IFACE), udp)
CFLAGS += -DCONFIG_CTRL_IFACE_UDP
endif
ifeq ($(CONFIG_CTRL_IFACE), named_pipe)
CFLAGS += -DCONFIG_CTRL_IFACE_NAMED_PIPE
endif
endif

ifdef QSDK_WRAPD
LIBS_c += -lqca_nl80211_wrapper
CFLAGS += -DCFG80211_ENABLE
endif
LIBS_c += $(LIBS)

ifndef LDO
LDO=$(CC)
endif

LDFLAGS += -pthread -lpthread -ldl -pie
CFLAGS += -DPROXY_NOACK_WAR -fpie

../src/drivers/build.wpa_supplicant:
	@if [ -e ../src/drivers/build.hostapd ]; then \
		$(MAKE) -C ../src/drivers clean; \
	fi
	@touch ../src/drivers/build.wpa_supplicant

BCHECK=../src/drivers/build.wpa_supplicant

wrapd: $(OBJS_c)
	$(LDO) $(LDFLAGS) -o wrapd $(OBJS_c) $(LIBS_c)

Q=@
E=echo
ifeq ($(V), 1)
Q=
E=true
endif

%.o: %.c
	$(Q)$(CC) -c -o $@ $(CFLAGS) $<
	@$(E) "  CC " $<

doc:
	doxygen
	make -C latex

clean:
	rm -f core *~ *.o *.d eap_*.so $(ALL)
	rm -rf latex html

-include $(OBJS:%.o=%.d)
