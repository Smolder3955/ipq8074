ALL= libloc_base_util.a
all: ${ALL}

clean:
	rm -f ${ALL}

install:
	@echo Nothing to be made.


ifndef CC
CC=gcc
endif

CFLAGS += -I. \
    -I.. 

BASE_UTIL_HEADERS=$(wildcard *.h)
BASE_UTIL_SRCS=$(wildcard src/*.cpp)
BASE_UTIL_OBJS=$(patsubst src/%.cpp,%.o,$(BASE_UTIL_SRCS))
%.o: src/%.cpp
	$(CXX) -c -fPIC -o $@ $(CFLAGS) ${COPTS} $<
	@echo "  CXX " $<

libloc_base_util.a: $(BASE_UTIL_OBJS)
	$(AR) crT $@ $?

-include $(BASE_UTIL_LIB_OBJS:%.o=%.d)
