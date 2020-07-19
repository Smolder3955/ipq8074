-include Makefile.defs
-include lbdConfig.defs

ifneq ($(strip $(TOOLPREFIX)),)
export  CROSS:=$(TOOLPREFIX)
endif

export CC = $(CROSS)gcc
export CFLAGS += -O2 -Wall -Werror
export CFLAGS += $(QCACFLAGS)

# Add postfix for builds from a git repository
ifndef LBD_VERSION_STR_POSTFIX
MAKEFILE_PATH := $(TOPDIR)/qca/src/qca-whc-lbd
GIT_SHA := $(shell cd $(MAKEFILE_PATH) && git rev-parse HEAD)
GIT_SHA_DATE := $(firstword $(shell cd $(MAKEFILE_PATH) && git --no-pager show --date=short --format="%ad" --name-only))
GIT_AU_VERSION := '$(shell git describe | sed -e 's/\. *.*//').${BRANCH}.${BUILD_NUMBER}'
GIT_AU_BUILD_DATE := $(shell date)
export CFLAGS += -DLBD_VERSION_STR_POSTFIX='" $(GIT_SHA) $(GIT_SHA_DATE)\n$(GIT_AU_VERSION) $(GIT_AU_BUILD_DATE)"'
endif

# Add Son Library flag
ifeq ($(LBD_SUPPORT_SONLIB),y)
ifneq ($(findstring 4.4, $(KERNELVERSION)),)
export CFLAGS += -DSONLIB_SUPPORT_ENABLED=0
else
export CFLAGS += -DSONLIB_SUPPORT_ENABLED=1
endif
else
export CFLAGS += -DSONLIB_SUPPORT_ENABLED=0
endif


ifeq ($(LBD_SUPPORT_CFG80211),y)
export CFLAGS += -DWIFISON_SUPPORT_ACK_RSSI
endif

ifeq ($(LBD_SUPPORT_QSDK),y)
export CFLAGS += -DWIFISON_SUPPORT_QSDK
endif

ifeq ($(LBD_SUPPORT_ATH10K),y)
export CFLAGS += -DWIFISON_SUPPORT_ATH10K
endif

export LDFLAGS += ${QCALDFLAGS}
export DEFS =
export OBJEXT = o
export EXEEXT =
export RANLIB = $(CROSS)ranlib
export STRIP = $(CROSS)strip
export ARFLAGS = cru
export AR = $(CROSS)ar
export COMPILE = $(CC) $(DEFS) $(INCLUDES) $(CFLAGS)
export LINK = $(CC) $(CFLAGS) $(LDFLAGS)

SRCDIRS = core \
         libs \
         shared \
         src

INSTALL_DIRS = lib \
             include

all: installdirs subdirs lbd_telnet
	@echo Made all in `pwd`

LOAD_BALANCING_DAEMON_TERMINAL := $(INSTALL_ROOT)/sbin/lbt

lbd_telnet:
	@if [ ! -f $(LOAD_BALANCING_DAEMON_TERMINAL) ]; then \
		echo "telnet 127.0.0.1 7787" > $(LOAD_BALANCING_DAEMON_TERMINAL); \
		chmod 755 $(LOAD_BALANCING_DAEMON_TERMINAL); \
	fi

installdirs :
	@if [ -d $(firstword $(SRCDIRS)) ]; then \
		for dir in $(INSTALL_DIRS); do (mkdir -p $(LB_INSTALL_ROOT)/$$dir); done ;\
	fi

subdirs : installdirs
	@if [ -d $(firstword $(SRCDIRS)) ]; then \
		for dir in $(SRCDIRS) ; do (cd $$dir && $(MAKE) all) || exit 1; done ;\
	fi

clean:
	@if [ -d $(firstword $(SRCDIRS)) ]; then \
		rm -rf install ;\
		for dir in $(SRCDIRS)  ; do (cd $$dir && $(MAKE) clean) || exit 1; done ;\
		find . -name *.o -o -name *.d | xargs rm -rf ;\
	fi

$(EXTERNAL_LIB):
	@make -C $(GWTOP) clean
	@make -C $(GWTOP) CC=$(TOOLPREFIX)gcc AR=$(TOOLPREFIX)ar LD=$(TOOLPREFIX)ld

.PHONY: all clean install

