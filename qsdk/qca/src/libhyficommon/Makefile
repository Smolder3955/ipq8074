# Makefile for libhyficommon
#
# -- .so (shared object) files will have unused objects removed
# Ultimately, the trimmed down .so file is used on the target.
#

# Shared Makefile stuff, place at BEGINNING of this Makefile

HYFICOMMONDIR := $(shell pwd)

ifneq ($(strip $(TOOLPREFIX)),)
export  CROSS:=$(TOOLPREFIX)
endif

export CC = $(CROSS)gcc
export CFLAGS += -O2 -Wall -Werror -fPIC
export CFLAGS += $(QCACFLAGS)
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

CFLAGS += -I. -I$(GWINCLUDE) -DDISABLE_DBG_PORT

HYFICOMMON_INSTALL_ROOT := $(HYFICOMMONDIR)/install

ifndef INSTALL_ROOT
INSTALL_ROOT=$(HYFICOMMON_INSTALL_ROOT)
endif

HYFICOMMON_INSTALL_INCLUDE = $(INSTALL_ROOT)/include
HYFICOMMON_INSTALL_LIB = $(INSTALL_ROOT)/lib

# What we build by default:
ALL=libhyficommon.so

OBJS = bufrd.o bufwr.o cmd.o csh.o dbg.o diag.o evloop.o interface.o split.o

# Making default targets:
all: local install
	@echo All done in `pwd`

local : $(ALL)
	@echo Made outputs in `pwd`

# Doing installation (see comments at top of this file)
# Note: header files should be installed with "ln -s -f" in order to
# prevent compiler from using old copies of local header files!
install: local
	@cp -a -f $(ALL) $(HYFICOMMON_INSTALL_LIB)
	@cp -a -f *.h $(HYFICOMMON_INSTALL_INCLUDE)
	@echo Installed outputs from `pwd`

# Making our specific library outputs
$(ALL) : $(OBJS)
	rm -f $@
	$(COMPILE) -shared -o $@ $(OBJS) $(LIBS)

# Remove all generated files
clean:
	@rm -f *.o *.so

.PHONY: all clean install

