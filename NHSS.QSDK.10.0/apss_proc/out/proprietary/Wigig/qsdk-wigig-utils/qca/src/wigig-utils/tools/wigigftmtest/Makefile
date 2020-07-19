-include $(TOPDIR)/rules.mk

ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

PWD := $(shell pwd)
CC = $(CROSS)gcc
CFLAGS = -Wall -g -MMD
LIBS :=
LOCAL_CFLAGS :=

ifndef INSTALL_ROOT
INSTALL_ROOT= $(WIGIGTOOLSDIR)/install
endif

LOCAL_CFLAGS += $(shell $(PKG_CONFIG) --cflags libnl-3.0)
LIBS += -lnl-3
LOCAL_CFLAGS += $(shell $(PKG_CONFIG) --cflags libnl-genl-3.0)
LIBS += -lnl-genl-3

WPA_DIR := $(TOPDIR)/qca/src/qca-hostap
LOCAL_CFLAGS += -I$(WPA_DIR)/src/common -I$(WPA_DIR)/src/drivers

LDFLAGS =
LIBS = -lwigigaoa -lm -lnl-3 -lnl-genl-3

wigigftmtest_srcs = ftmtest.c
wigigftmtest_objs :=$(wigigftmtest_srcs:.c=.o)
wigigftmtest_all_objs :=$(wigigftmtest_objs)

progs = wigigftmtest
# What we build by default:
ALL = $(progs)

all prod prof: $(progs) install

wigigftmtest: $(wigigftmtest_objs)

prod prof: CFLAGS += -O2
prof: CFLAGS += -pg

prod: strip

$(wigigftmtest_all_objs): %.o: %.c

$(wigigftmtest_objs):
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
	rm -rf install/usr/sbin/wigigftmtest
	$(RM) $(wigigftmtest_all_objs) $(progs) $(wigigftmtest_all_objs:%.o=%.d)

echo:
	@echo $(progs) $(wigigftmtest_srcs) $(wigigftmtest_all_objs) $(wigigftmtest_all_objs:%.o=%.d)

-include $(wigigftmtest_all_objs:%.o=%.d)
