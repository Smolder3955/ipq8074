# Makefile for libwpa2

LIBWPA2DIR := $(shell pwd)

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

CFLAGS += -I. -DCONFIG_INTERNAL_LIBTOMMATH -DEAP_WPS -DINTERNAL_MD5 -DINTERNAL_SHA1 \
          -DINTERNAL_SHA256 -DCONFIG_INTERNAL_X509 -DINTERNAL_AES -DEAP_TLS_FUNCS   \
          -DCONFIG_TLS_INTERNAL -DCONFIG_CRYPTO_INTERNAL -DINTERNAL_DES

LIBWPA2_INSTALL_ROOT := $(LIBWPA2DIR)/install

ifndef INSTALL_ROOT
INSTALL_ROOT=$(LIBWPA2_INSTALL_ROOT)
endif

LIBWPA2_INSTALL_INCLUDE = $(INSTALL_ROOT)/include
LIBWPA2_INSTALL_LIB = $(INSTALL_ROOT)/lib

# What we build by default:
ALL=libwpa2.so

OBJS = eloop.o wps_parser.o os_unix.o aes_wrap.o sha256.o crypto_internal.o bignum.o sha1.o rsa.o common.o rc4.o asn1.o md5.o des.o

# Making default targets:
all: local install
	@echo All done in `pwd`

local : $(ALL)
	@echo Made outputs in `pwd`

# Doing installation (see comments at top of this file)
# Note: header files should be installed with "ln -s -f" in order to
# prevent compiler from using old copies of local header files!
install: local
	mkdir -p $(LIBWPA2_INSTALL_LIB)
	@cp -a -f $(ALL) $(LIBWPA2_INSTALL_LIB)
	mkdir -p $(LIBWPA2_INSTALL_INCLUDE)
	@cp -a -f *.h $(LIBWPA2_INSTALL_INCLUDE)
	@echo Installed outputs from `pwd`

# Making our specific library outputs
$(ALL) : $(OBJS)
	rm -f $@
	$(COMPILE) -shared -o $@ $(OBJS) $(LIBS)

# Remove all generated files
clean:
	@rm -f *.o *.so

.PHONY: all clean install

