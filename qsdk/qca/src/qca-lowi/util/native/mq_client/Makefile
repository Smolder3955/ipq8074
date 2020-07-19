ALL= libloc_mq_client.a
all: ${ALL}

clean:
	rm -f ${ALL}

install:
	@echo Nothing to be made.


ifndef CC
CC=gcc
endif

../base_util/libloc_base_util.a:
	$(MAKE) -C ../base_util

CFLAGS += -I. \
    -I.. 

STATIC_LIBS= \
  ../base_util/libloc_base_util.a

SRCS=$(wildcard src/*.cpp)
OBJS=$(patsubst src/%.cpp,%.o,$(SRCS))
%.o: src/%.cpp
	$(CXX) -c -fPIC -o $@ $(CFLAGS) ${COPTS} $<
	@echo "  CXX " $<

libloc_mq_client.a: $(OBJS) $(STATIC_LIBS)
	$(AR) crT $@ $?

-include $(BASE_UTIL_LIB_OBJS:%.o=%.d)
