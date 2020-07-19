# ===========================================================================
# NSS distro configuration targets
# These targets are used from top-level makefile

# ===========================================================================
# Shared Makefile for the various kconfig executables:
# conf:	  Used for defconfig, oldconfig and related targets
# mconf:  Used for the mconfig target.
#         Utilizes the lxdialog package
# object files used by all kconfig flavours

# Platform specific fixes
#
# FreeBSD
REL_DATE := $(shell date +%Y-%m-%d)
export CFLAGS+=-DKBUILD_NO_NLS -DBUILDATE=\"$(REL_DATE)\"
BIN := bin
MD := mkdir
CP := cp
RM := rm
SED := sed

conf-objs	:= conf.o zconf.tab.o
mconf-objs	:= mconf.o zconf.tab.o

clean-files	:= lkc_defs.h qconf.moc .tmp_qtcheck \
		   .tmp_gtkcheck zconf.tab.c lex.zconf.c zconf.hash.c

INSTALL_PROGRAM := install -D

all: $(BIN)/nss_mconf $(BIN)/nss_conf $(BIN)/nss_lxdialog

$(BIN)/nss_mconf: mconf
	$(MD) -p $(BIN)
	$(CP) $< $@

$(BIN)/nss_conf: conf
	$(MD) -p $(BIN)
	$(CP) $< $@

$(BIN)/nss_lxdialog: lxdialog/lxdialog
	$(MD) -p $(BIN)
	$(CP) $< $@

lxdialog/lxdialog:
	$(MAKE) -C lxdialog

conf: $(conf-objs)

mconf: $(mconf-objs)

clean:
	$(RM) -f *.o $(clean-files) mconf conf
	$(RM) -f bin/*
	$(MAKE) -C lxdialog clean

zconf.tab.o: lex.zconf.c zconf.hash.c confdata.c

kconfig_load.o: lkc_defs.h

lkc_defs.h: $(src)/lkc_proto.h
	$(SED) < $< > $@ 's/P(\([^,]*\),.*/#define \1 (\*\1_p)/'

zconf.tab.c: zconf.y
lex.zconf.c: zconf.l
zconf.hash.c: zconf.gperf

%.tab.c: %.y
	$(CP) $@_shipped $@ || bison -l -b $* -p $(notdir $*) $<

lex.%.c: %.l
	$(CP) $@_shipped $@ || flex -L -P$(notdir $*) -o$@ $<

%.hash.c: %.gperf
	$(CP) $@_shipped $@ || gperf < $< > $@

.PHONY: all clean
