MKDIR	= @mkdir -p $(@D)
SRCPATH	= ./
OBJPATH	= obj

BINARY	= $(OBJPATH)/libnl-nss.so
SOURCES = $(wildcard $(SRCPATH)/*.c)
OBJECTS = $(SOURCES:$(SRCPATH)/%.c=$(OBJPATH)/%.o)
HEADERS = $(wildcard $(SRCPATH)/*.h)

INCLUDE += -I../../qca-nss-clients/netlink/include
INCLUDE += -I../../qca-nss-drv/exports
INCLUDE += -I./include
EXTRA_CFLAGS = -Wall -Werror -fPIC

all: release

release: $(BINARY)

$(OBJPATH)/%.o: $(SRCPATH)/%.c $(HEADERS)
	$(MKDIR)
	@echo [CC] $@
	@$(CC) -c $(CFLAGS) $(INCLUDE) $(EXTRA_CFLAGS) -o $@ $<

$(BINARY): $(OBJECTS)
	@echo $(BINARY)
	@echo [LD] $@
	@echo $(AR) rcs $@ $^ $(LDFLAGS) $(EXTRA_LDFLAGS) $(LDLIBS).....
#@$(AR) rcs $@ $^ $(LDFLAGS) $(EXTRA_LDFLAGS) $(LDLIBS)
	@$(CC) -shared -o $@ $^ $(LDFLAGS) $(EXTRA_LDFLAGS) $(LDLIBS)

clean:
	@echo [Clean]
	@rm -f $(OBJECTS)
	@rm -f $(BINARY)
	@rm -rf $(OBJPATH)

.PHONY: clean
