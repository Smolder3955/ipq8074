ALL= liblowi_client.so  install
all: ${ALL}

clean:
	rm -f ${ALL}

ifndef CC
CC=gcc
endif

ifndef CFLAGS
CFLAGS = -MMD -O2 -Wall -g
endif

CFLAGS += -I. \
    -I.. \
    -I../.. \
    -I$(LOCAL_PATH_UTILS)

STATIC_LIBS= \
  $(LOCAL_PATH_UTILS)/mq_client/libloc_mq_client.a

$(LOCAL_PATH_UTILS)/mq_client/libloc_mq_client.a:
	$(MAKE) -C $(LOCAL_PATH_UTILS)/mq_client

LIB_OBJS=$(patsubst %.cpp,%.o,$(wildcard *.cpp))
LIB_OBJS+=$(patsubst %.cpp,%.o,$(wildcard ../common/*.cpp))
%.o: %.cpp
	$(CXX) -c -fPIC -o $@ $(CFLAGS) ${COPTS} $<
	@echo "  CXX " $<
../common/%.o: ../common/%.cpp
	$(CXX) -c -o $@ ${CXXFLAGS} $(CFLAGS) ${COPTS} $<
	@echo "  CXX " $<

liblowi_client.so: $(LIB_OBJS) $(STATIC_LIBS)
	$(CXX) -shared -fPIC -Wl,-soname,$@ -o $@ -Wl,-whole-archive $? -Wl,-no-whole-archive -lc

install:
	cp -a -f liblowi_client.so $(INSTALL_ROOT)/lib

-include $(BASE_UTIL_LIB_OBJS:%.o=%.d)
