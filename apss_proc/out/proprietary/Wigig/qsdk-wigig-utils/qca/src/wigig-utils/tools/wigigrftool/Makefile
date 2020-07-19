-include $(TOPDIR)/rules.mk

PWD := $(shell pwd)
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

progs := wigigrftool

LOCAL_CFLAGS += $(shell $(PKG_CONFIG) --cflags libnl-3.0)
LIBS += -lnl-3
LOCAL_CFLAGS += $(shell $(PKG_CONFIG) --cflags libnl-genl-3.0)
LIBS += -lnl-genl-3

WPA_DIR := $(TOPDIR)/qca/src/qca-hostap
LOCAL_CFLAGS += -I$(WPA_DIR)/src/common -I$(WPA_DIR)/src/drivers

# What we build by default:
ALL = $(progs)

wigigrftool_srcs = wigigrftool.c
wigigrftool_objs :=$(wigigrftool_srcs:.c=.o)
wigigrftool_all_objs :=$(wigigrftool_objs)

all prod prof: $(progs) install

wigigrftool: $(wigigrftool_objs)

prod prof: CFLAGS += -O2
prof: CFLAGS += -pg

prod: strip

$(wigigrftool_all_objs): %.o: %.c

$(wigigrftool_objs):
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
	rm -rf install/usr/sbin/wigigrftool
	$(RM) $(wigigrftool_all_objs) $(progs) $(wigigrftool_all_objs:%.o=%.d)

echo:
	@echo $(progs) $(wigigrftool_srcs) $(wigigrftool_all_objs) $(wigigrftool_all_objs:%.o=%.d)

-include $(wigigrftool_all_objs:%.o=%.d)
