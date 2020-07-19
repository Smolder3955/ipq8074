TOP_SRCDIR = ../../
include $(TOP_SRCDIR)/Makefile.defs

INSTALLHEADERS = stadb.h stadbCmn.h stadbEntry.h

# These are shared and exported for other daemons to use.
TARGET_LIB = libstadb.a
TARGET_OBJECTS = stadbHashTable.$(OBJEXT) stadbEntry.$(OBJEXT) \
                 stadb.$(OBJEXT)

# These are not shared/exported to other daemons.
BSA_TARGET_LIB = libstadb_bsa.a
BSA_TARGET_SOURCES = stadbBSA.c
BSA_TARGET_OBJECTS = $(patsubst %.c,%.$(OBJEXT),$(BSA_TARGET_SOURCES))

SHARED_CFLAGS += -fPIC

all: $(TARGET_LIB) install

$(TARGET_LIB): $(TARGET_OBJECTS)
	@rm -f $(TARGET_LIB)
	$(AR) $(ARFLAGS) $(TARGET_LIB) $(TARGET_OBJECTS)
	$(RANLIB) $(TARGET_LIB)

$(BSA_TARGET_LIB): $(BSA_TARGET_OBJECTS)
	@rm -f $(BSA_TARGET_LIB)
	$(AR) $(ARFLAGS) $(BSA_TARGET_LIB) $(BSA_TARGET_OBJECTS)
	$(RANLIB) $(BSA_TARGET_LIB)

.c.o:
	$(COMPILE) $(CFLAGS) $(SHARED_CFLAGS) -c $<

install: $(TARGET_LIB) $(BSA_TARGET_LIB)
	@cp -a -f $(TARGET_LIB) $(BSA_TARGET_LIB) $(LBLIB_STATIC)/.
	@for file in $(INSTALLHEADERS) ; do ln -s -f `pwd`/$$file $(LBINCLUDE)/. ; done

clean:
	@rm -f *.o $(TARGET_LIB) $(BSA_TARGET_LIB) *~

.PHONY: all clean
