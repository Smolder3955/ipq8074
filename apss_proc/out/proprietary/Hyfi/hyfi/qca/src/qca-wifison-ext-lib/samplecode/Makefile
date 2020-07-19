WIFISONEVENTSAMPLEDIR := $(shell pwd)

ifneq ($(strip $(TOOLPREFIX)),)
export  CROSS:=$(TOOLPREFIX)
endif


export INCLUDES = -I$(WIFISONEVENTSAMPLEDIR)/../
export INCLUDES += -I$(EXTRA_CFLAGS)
export CC = $(CROSS)gcc
export CFLAGS += -O2 -Wall -Werror -fPIC
export CFLAGS += $(QCACFLAGS)
export LDFLAGS += ${QCALDFLAGS}
export LDFLAGS += ${EXTRA_LDFLAGS}
export DEFS =
export OBJEXT = o
export EXEEXT =
export RANLIB = $(CROSS)ranlib
export STRIP = $(CROSS)strip
export ARFLAGS = cru
export AR = $(CROSS)ar
export COMPILE = $(CC) $(DEFS) $(INCLUDES) $(CFLAGS)
export LINK = $(CC) $(CFLAGS) $(LDFLAGS)

LIBS += -lqca_wifison_ext

EVENT_OBJECTS = qca_event_sample.o
LISTEN_OBJECTS = qca_listen_port.o
GENL_EVENT_OBJECTS = qca_genlevent_sample.o


all: qca_event_sample$(EXEEXT) qca_listen_port$(EXEEXT) qca_genlevent_sample$(EXEEXT) install

qca_event_sample$(EXEEXT): $(EVENT_OBJECTS)
	$(LINK) -o $@ $(EVENT_OBJECTS) -Xlinker --start-group $(LIBS) -Xlinker --end-group

qca_listen_port$(EXEEXT): $(LISTEN_OBJECTS)
	$(LINK) -o $@ $(LISTEN_OBJECTS) -Xlinker --start-group $(LIBS) -Xlinker --end-group

qca_genlevent_sample$(EXEEXT): $(GENL_EVENT_OBJECTS)
	$(LINK) -o $@ $(GENL_EVENT_OBJECTS) -Xlinker --start-group $(LIBS) -Xlinker --end-group

.c.o:
	$(COMPILE) -c $<

install:
	@cp qca_event_sample$(EXEEXT) $(WIFISONEVENTSAMPLEDIR)/../install/sbin
	@cp qca_listen_port$(EXEEXT) $(WIFISONEVENTSAMPLEDIR)/../install/sbin
	@cp qca_genlevent_sample$(EXEEXT) $(WIFISONEVENTSAMPLEDIR)/../install/sbin

clean:
	@rm -f *.o *~ hyd$(EXEEXT)

.PHONY: all clean install
