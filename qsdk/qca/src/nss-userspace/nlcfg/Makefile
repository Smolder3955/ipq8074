MKDIR	= mkdir -p $(@D)
SRCPATH	= src
OBJPATH	= obj
SRCDIR = ./

BINARY	= $(OBJPATH)/nlcfg
SOURCES = $(wildcard $(SRCDIR)/src/*.c)
HEADERS = $(wildcard $(SRCDIR)/include/*.h)
OBJECTS = $(SOURCES:$(SRCDIR)/src/%.c=$(OBJPATH)/%.o)

INCLUDE = -I../../qca-nss-clients/netlink/include
INCLUDE += -I../../qca-nss-drv/exports
INCLUDE += -I./include
INCLUDE += -I../lib/include
EXTRA_CFLAGS = -Wall -Werror
EXTRA_LDFLAGS = -lnl-genl-3 -lnl-nss
LDLIBS  = -L../lib/obj

all: release

release: $(BINARY)


$(OBJPATH)/%.o: $(SRCPATH)/%.c $(HEADERS)
	$(MKDIR)
	@echo [CC] $@
	@$(CC) -c $(CFLAGS) $(INCLUDE) $(EXTRA_CFLAGS) -o $@ $<

$(BINARY): $(OBJECTS)
	@echo $(BINARY)
	@echo [LD] $@
	@$(CC) -o $@ $^ $(LDFLAGS) $(EXTRA_LDFLAGS) $(LDLIBS)
clean:
	@echo [Clean]
	@rm -f $(OBJECTS)
	@rm -f $(BINARY)
	@rmdir $(OBJPATH)

.PHONY: clean
