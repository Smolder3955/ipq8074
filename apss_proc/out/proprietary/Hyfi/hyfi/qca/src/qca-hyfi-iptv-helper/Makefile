################################################################################
#
# QCA HyFi Netfilter for IPTV implementations on Wireless interface
# Copyright (c) 2013 Qualcomm Atheros, Inc.
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
################################################################################

# Target module name
TARGET := hyfi-iptv-helper

# Kernel directory - This works only for the host.
# The target kernel is different, and it can't be detected.
ifndef KERNELPATH
KERNELPATH := /lib/modules/$(shell uname -r)/build
endif

# Include directories
INCLUDES := -I$(CURDIR) \
    -I$(KERNELPATH)/include/linux \
    -I$(KERNELPATH)/include/asm \
    -I$(KERNELPATH)/net/bridge \
    -I$(KBUILDPATH)/include/linux \
    -I$(KBUILDPATH)/include/asm \
    -I$(KBUILDPATH)/net/bridge 

export EXTRA_CFLAGS += $(INCLUDES)

# Module extra compilation flags
EXTRA_CFLAGS += -Werror -Wall -g

# Kernel module compilation - part 2
ifneq ($(KERNELRELEASE),)
obj-m += $(TARGET).o

# Target objects - add as many as required
$(TARGET)-objs := \
	hyfi_iptv_main.o

else 

# Makefile targets - part 1

all: build install

build:
	@$(MAKE) -C $(KERNELPATH) M=`pwd` modules

install:
	@install -m 644 $(TARGET).ko $(MODULEPATH)

uninstall:
	@find $(MODULEPATH) -name $(TARGET).ko | xargs rm -rf

clean:
	@$(MAKE) -C $(KERNELPATH) M=`pwd` clean
	@rm -f Module.symvers

endif
