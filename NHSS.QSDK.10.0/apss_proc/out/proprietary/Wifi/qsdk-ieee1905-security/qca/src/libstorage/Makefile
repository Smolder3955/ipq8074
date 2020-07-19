# Makefile for libstorage
#
# -- .so (shared object) files will have unused objects removed
# Ultimately, the trimmed down .so file is used on the target.
#

# Shared Makefile stuff, place at BEGINNING of this Makefile

STORAGEDIR := $(shell pwd)

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

STORAGE_INSTALL_ROOT := $(STORAGEDIR)/install

ifndef INSTALL_ROOT
INSTALL_ROOT=$(STORAGE_INSTALL_ROOT)
endif

STORAGE_INSTALL_INCLUDE = $(INSTALL_ROOT)/include
STORAGE_INSTALL_LIB = $(INSTALL_ROOT)/lib

GWLIB = $(INSTALL_ROOT)/lib
LIBS += -lhyficommon -lwifisoncfg
CFLAGS += -I. -I$(GWINCLUDE) -L$(GWLIB) -I$(ATHDIR)

# What we build by default:
ALL=libstorage.so

# For every C file to be compiled, place it's object file in the OBJS list
# (as required by Makefile.rules).
# Please keep in alphabetical order
ifeq ($(LIB_STORAGE_USE_INI_FORMAT), y)
OBJS = storage_ini.o
else 

ifeq ($(LIB_STORAGE_USE_UCI_FORMAT), y)
OBJS = storage_uci.o uci/package.o uci/uci_core.o uci/uci_wsplc.o uci/uci_wireless.o uci/uci_hyfi.o \
	uci/uci_plc.o
else
OBJS = storage.o
endif

endif

# Making default targets:
all: local install
	@echo All done in `pwd`

local : $(ALL)
	@echo Made outputs in `pwd`

# Doing installation (see comments at top of this file)
# Note: header files should be installed with "ln -s -f" in order to
# prevent compiler from using old copies of local header files!
install: local
	cp -a -f $(ALL) $(STORAGE_INSTALL_LIB)
	cp -a -f storage.h $(STORAGE_INSTALL_INCLUDE)
	@echo Installed outputs from `pwd`

# Making our specific library outputs
$(ALL) : $(OBJS)
	rm -f $@
	$(COMPILE) -shared -o $@ $(OBJS) $(LIBS)

# Remove all generated files
clean:
	@rm -f *.o *.so

.PHONY: all clean install

