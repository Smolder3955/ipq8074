# Makefile for libieee1905
#
# -- .so (shared object) files will have unused objects removed
# Ultimately, the trimmed down .so file is used on the target.
#

# Shared Makefile stuff, place at BEGINNING of this Makefile

IEEE1905DIR := $(shell pwd)

ifneq ($(strip $(TOOLPREFIX)),)
export  CROSS:=$(TOOLPREFIX)
endif

export CC = $(CROSS)gcc
export CFLAGS += -O2 -Wall -Werror -fPIC $(EXTRA_CFLAGS)
export DEFS = 
export OBJEXT = o
export EXEEXT = 
export RANLIB = $(CROSS)ranlib
export STRIP = $(CROSS)strip
export ARFLAGS = cru
export AR = $(CROSS)ar
export COMPILE = $(CC) $(DEFS) $(INCLUDES) $(CFLAGS)
export LINK = $(CC) $(CFLAGS) $(EXTRA_LDFLAGS)

CFLAGS += -I. -I$(GWINCLUDE)

IEEE1905_INSTALL_ROOT := $(IEEE1905DIR)/install

ifndef INSTALL_ROOT
INSTALL_ROOT=$(IEEE1905_INSTALL_ROOT)
endif

IEEE1905_INSTALL_INCLUDE = $(INSTALL_ROOT)/include
IEEE1905_INSTALL_LIB = $(INSTALL_ROOT)/lib

# What we build by default:
ALL=libieee1905.so

OBJS = ieee1905.o mid.o

# Making default targets:
all: local install
	@echo All done in `pwd`

local : $(ALL)
	@echo Made outputs in `pwd`

# Doing installation (see comments at top of this file)
# Note: header files should be installed with "ln -s -f" in order to
# prevent compiler from using old copies of local header files!
install: local
	@cp -a -f $(ALL) $(IEEE1905_INSTALL_LIB)
	@cp -a -f mid.h $(IEEE1905_INSTALL_INCLUDE)
	@cp -a -f ieee1905.h $(IEEE1905_INSTALL_INCLUDE)
	@cp -a -f ieee1905_defs.h $(IEEE1905_INSTALL_INCLUDE)
	@cp -a -f ieee1905_vendor.h $(IEEE1905_INSTALL_INCLUDE)
	@cp -a -f ieee1905_vendor_consts.h $(IEEE1905_INSTALL_INCLUDE)
	@echo Installed outputs from `pwd`

# Making our specific library outputs
$(ALL) : $(OBJS)
	rm -f $@
	$(COMPILE) -shared -o $@ $(OBJS) $(LIBS)

# Remove all generated files
clean:
	@rm -f *.o *.so

.PHONY: all clean install

