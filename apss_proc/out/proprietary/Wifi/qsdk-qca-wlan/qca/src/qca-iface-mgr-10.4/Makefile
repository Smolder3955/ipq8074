CC=$(TOOLPREFIX)gcc

CFLAGS += -MMD -O2 -Wall -Werror -g


ALL=iface-mgr

all: $(ALL)

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	for i in $(ALL); do cp $$i $(DESTDIR)$(BINDIR)/$$i; done


OBJS_c = main.o iface_mgr_api.o drv_nl80211.o $(WPA_PATH)/wpa_ctrl.o
OBJS_c += $(WPA_PATH)/netlink.o
OBJS_c += $(WPA_PATH)/eloop.o
OBJS_c += $(WPA_PATH)/wpa_debug.o
OBJS_c += $(WPA_PATH)/common.o
OBJS_c += son.o


ifndef CONFIG_OS
CONFIG_OS=unix
endif

ifeq ($(CONFIG_OS), internal)
CFLAGS += -DOS_NO_C_LIB_DEFINES
endif

OBJS_c += $(WPA_PATH)/os_$(CONFIG_OS).o
LDFLAGS += -lpthread -ldl
ifdef CONFIG_WPA_TRACE
CFLAGS += -DWPA_TRACE


OBJS_c += $(WPA_PATH)/trace.o
OBJS_c += $(WPA_PATH)/wpa_debug.o

LDFLAGS += -rdynamic
CFLAGS += -funwind-tables
ifdef CONFIG_WPA_TRACE_BFD
CFLAGS += -DWPA_TRACE_BFD
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

ifeq ($(IFACE_SUPPORT_CFG80211),y)
LDFLAGS += -lwifisoncfg -lhyficommon
LDFLAGS += -lqca_nl80211_wrapper -lnl-3 -lnl-genl-3 -lxml2
endif

LIBS_c += $(LIBS)

ifndef LDO
LDO=$(CC)
endif

../src/drivers/build.wpa_supplicant:
	@if [ -e ../src/drivers/build.hostapd ]; then \
		$(MAKE) -C ../src/drivers clean; \
	fi
	@touch ../src/drivers/build.wpa_supplicant

BCHECK=../src/drivers/build.wpa_supplicant

iface-mgr: $(OBJS_c)
	$(LDO) $(LDFLAGS) -o iface-mgr $(OBJS_c) $(LIBS_c)

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
