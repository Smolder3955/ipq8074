-include $(TOPDIR)/rules.mk

CFLAGS := -Wall -g -MMD
LDFLAGS :=
LIBS :=
LOCAL_CFLAGS :=

ifneq ($(strip $(TOOLPREFIX)),)
export  CROSS:=$(TOOLPREFIX)
endif

ifndef INSTALL_ROOT
INSTALL_ROOT= $(WIGIGTOOLSDIR)/install
endif

export CC = $(CROSS)gcc
CFLAGS += -L$(INSTALL_ROOT)/lib

PKG_CONFIG := pkg-config

progs := wigigaptest

LOCAL_CFLAGS += $(shell $(PKG_CONFIG) --cflags libnl-3.0)
LIBS += -lnl-3
LOCAL_CFLAGS += $(shell $(PKG_CONFIG) --cflags libnl-genl-3.0)
LIBS += -lnl-genl-3

# What we build by default:
ALL = $(progs)

wigigap_srcs = wigigaptest.c
wigigap_objs :=$(wigigap_srcs:.c=.o)
wigigap_all_objs :=$(wigigap_objs)

all prod prof: $(progs) install

wigigaptest: $(wigigap_objs)

prod prof: CFLAGS += -O2
prof: CFLAGS += -pg

prod: strip

$(wigigap_all_objs): %.o: %.c

$(wigigap_objs):
	$(CC) $(CFLAGS) $(LOCAL_CFLAGS) -o $@ -c $<

$(progs): %:
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

strip:
	strip $(progs)

# Doing installation (see comments at top of this file)
install:
	mkdir -p $(INSTALL_ROOT)/usr/sbin/
	cp -a -f $(ALL) $(INSTALL_ROOT)/usr/sbin/
	@echo Installed outputs from `pwd`


clean:
	rm -rf install/usr/sbin/wigigaptest
	$(RM) $(wigigap_all_objs) $(progs) $(wigigap_all_objs:%.o=%.d)

echo:
	@echo $(progs) $(wigigap_srcs) $(wigigap_all_objs) $(wigigap_all_objs:%.o=%.d)

-include $(wigigap_all_objs:%.o=%.d)
