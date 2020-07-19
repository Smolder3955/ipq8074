# Copyright (c) 2012, 2015 Qualcomm Atheros, Inc.
# All rights reserved.
# Qualcomm Atheros Confidential and Proprietary.

-include Makefile.defs

ifneq ($(strip $(TOOLPREFIX)),)
export  CROSS:=$(TOOLPREFIX)
endif

export CC = $(CROSS)gcc
#no-error=parentheses has been added as a temporary fix, for an issue in GCC header file endian.h
#This should be removed once this issue is fixed in endian.h file
export CFLAGS += -O2 -Wall -Werror -Wno-error=parentheses
export DEFS =
export OBJEXT = o
export EXEEXT =
export RANLIB = $(CROSS)ranlib
export STRIP = $(CROSS)strip
export ARFLAGS = cru
export AR = $(CROSS)ar
export COMPILE = $(CC) $(DEFS) $(INCLUDES) $(CFLAGS) $(TARGET_CFLAGS)
export LINK = $(CC) $(LDFLAGS) $(TARGET_LDFLAGS)

SRCDIRS = core \
	  managers \
	  services \
	  src \
	  tools

INSTALL_DIRS = lib \
             include

EXTERNAL_LIB = $(GWLIB)/libath.so

all:$(EXTERNAL_LIB) installdirs subdirs mcs_telnet
	@echo Made all in `pwd`

MCS_DAEMON_TERMINAL := $(INSTALL_ROOT)/sbin/mcst

mcs_telnet:
	@if [ ! -f $(MCS_DAEMON_TERMINAL) ]; then \
		echo "telnet 127.0.0.1 10000" > $(MCS_DAEMON_TERMINAL); \
		chmod 755 $(MCS_DAEMON_TERMINAL); \
	fi

installdirs :
	@if [ -d $(firstword $(SRCDIRS)) ]; then \
		for dir in $(INSTALL_DIRS); do (mkdir -p $(MCS_INSTALL_ROOT)/$$dir); done ;\
	fi

subdirs : installdirs
	@$(MAKE) -C plugins all
	@if [ -d $(firstword $(SRCDIRS)) ]; then \
		for dir in $(SRCDIRS) ; do (cd $$dir && $(MAKE) all) || exit 1; done ;\
	fi

clean:
	@if [ -d $(firstword $(SRCDIRS)) ]; then \
		rm -rf install ;\
		for dir in $(SRCDIRS)  ; do (cd $$dir && $(MAKE) clean) || exit 1; done ;\
		find . -name *.o -o -name *.d | xargs rm -rf ;\
	fi
	@$(MAKE) -C plugins clean

$(EXTERNAL_LIB):
	@make -C $(GWTOP) clean
	@make -C $(GWTOP) CC=$(TOOLPREFIX)gcc AR=$(TOOLPREFIX)ar LD=$(TOOLPREFIX)ld

.PHONY: all clean install

