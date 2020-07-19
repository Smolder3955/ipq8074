ALL= libloc_mq_server.a
all: ${ALL}

clean:
	rm -f ${ALL}

install:
	@echo Nothing to be made.


ifndef CC
CC=gcc
endif

ifndef CFLAGS
CFLAGS = -MMD -O2 -Wall -g
endif

CFLAGS+= -I.. -I. 

SRCS=$(wildcard *.cpp)
OBJS=$(patsubst %.cpp,%.o,$(SRCS))
%.o: %.cpp
	$(CXX) -c -o $@ $(CFLAGS) ${COPTS} $<
	@echo "  CXX " $<

libloc_mq_server.a: $(OBJS)
	$(AR) crT $@ $?

-include $(BASE_UTIL_LIB_OBJS:%.o=%.d)
