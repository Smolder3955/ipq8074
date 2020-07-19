# Makefile for libhyfi_bridge
#
# -- .so (shared object) files will have unused objects removed
# Ultimately, the trimmed down .so file is used on the target.
#
# Makefile.defs needs GWTOP defined!
include Makefile.defs

# Making default targets:
all: local install
	@echo All done in `pwd`

# Shared Makefile stuff, place at BEGINNING of this Makefile

HYFIBRIDGEDIR := $(shell pwd)

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

HYFIBRIDGE_INSTALL_ROOT := $(HYFIBRIDGEDIR)/install

ifndef INSTALL_ROOT
INSTALL_ROOT=$(HYFIBRIDGE_INSTALL_ROOT)
endif

HYFIBRIDGE_INSTALL_INCLUDE = $(INSTALL_ROOT)/include
HYFIBRIDGE_INSTALL_LIB = $(INSTALL_ROOT)/lib


# What we build by default:
ALL=libhyfi-bridge.so

# For every C file to be compiled, place it's object file in the OBJS list
# (as required by Makefile.rules).
# Please keep in alphabetical order
OBJS = \
    hyfi-bridge.o

local : $(ALL)
	@echo Made outputs in `pwd`

# Doing installation (see comments at top of this file)
# Note: header files should be installed with "ln -s -f" in order to
# prevent compiler from using old copies of local header files!
install:
	@cp -a -f $(ALL) $(HYFIBRIDGE_INSTALL_LIB)
	@cp -a -f hyfi-bridge.h $(HYFIBRIDGE_INSTALL_INCLUDE)
	@echo Installed outputs from `pwd`

# Making our specific library outputs
$(ALL) : $(OBJS)
	@$(COMPILE) -shared -o $@ $(OBJS)

# Remove all generated files
clean:
	@rm -f *.o *.so

.PHONY: all clean install

