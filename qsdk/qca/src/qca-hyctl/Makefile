# Copyright (c) 2010, Atheros Communications Inc.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

include Makefile.defs

ifneq ($(strip $(TOOLPREFIX)),)
export  CROSS:=$(TOOLPREFIX)
endif

export CC = $(CROSS)gcc
export CFLAGS += -O2  -Wall -Werror $(EXTRA_CFLAGS)
export DEFS = 
export OBJEXT = o
export EXEEXT = 
export RANLIB = $(CROSS)ranlib
export STRIP = $(CROSS)strip
export ARFLAGS = cru
export AR = $(CROSS)ar
export COMPILE = $(CC) $(DEFS) $(INCLUDES) $(CFLAGS)
export LINK = $(CC) $(CFLAGS) $(LDFLAGS) $(EXTRA_LDFLAGS)

CFLAGS += -I$(GWINCLUDE)

HYCTL_INSTALL_ROOT := $(HYCTLDIR)/install

ifndef INSTALL_ROOT
INSTALL_ROOT=$(HYCTL_INSTALL_ROOT)
endif

TARGET := hyctl$(EXEEXT)
TARTGET_OBJECTS := hyctl.$(OBJEXT)

all: $(TARGET) install
	@echo Made all in `pwd`

ifdef TARGET
$(TARGET): $(TARTGET_OBJECTS)
	$(LINK) -o $@ $(TARTGET_OBJECTS) $(LIBS)
endif	

.c.o:
	$(COMPILE) -c $<


clean: 
	rm -f *.o $(TARGET)

install:
ifdef TARGET
	cp $(TARGET) $(INSTALL_ROOT)
endif	

.PHONY: all clean install
	
