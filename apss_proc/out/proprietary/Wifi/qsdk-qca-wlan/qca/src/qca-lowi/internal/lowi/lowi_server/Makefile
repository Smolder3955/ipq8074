ifneq ($(strip $(TOOLPREFIX)),)
CROSS:=$(TOOLPREFIX)
endif

CC= $(CROSS)gcc

INC_DIRS+= -I. \
    -I./wifidriver/fakewifidriver \
    -I./wifidriver/util \
    -I./wifidriver/romewifidriver \
    -I.. \
    -I../inc \
    -I../common \
    -I../.. \
    -I$(LOCAL_PATH_UTILS)

STATIC_LIBS= \
  $(LOCAL_PATH_UTILS)/base_util/libloc_base_util.a \
  $(LOCAL_PATH_UTILS)/mq_client/libloc_mq_client.a \
  $(LOCAL_PATH_UTILS)/mq_server/libloc_mq_server.a \
  wifidriver/romewifidriver/libwifidriver_rome.a \
  wifidriver/util/libwifidriver_util.a

CFLAGS+= ${INC_DIRS} -MMD -O2 -g -lpthread -lnl-3 -lnl-genl-3

$(LOCAL_PATH_UTILS)/base_util/libloc_base_util.a:
	$(MAKE) -C $(LOCAL_PATH_UTILS)/base_util
$(LOCAL_PATH_UTILS)/mq_client/libloc_mq_client.a:
	$(MAKE) -C $(LOCAL_PATH_UTILS)/mq_client
$(LOCAL_PATH_UTILS)/mq_server/libloc_mq_server.a:
	$(MAKE) -C $(LOCAL_PATH_UTILS)/mq_server
wifidriver/romewifidriver/libwifidriver_rome.a:
	$(MAKE) -C wifidriver/romewifidriver
wifidriver/util/libwifidriver_util.a:
	$(MAKE) -C wifidriver/util

HEADERS=$(wildcard *.h)
COMMON_HEADERS=$(wildcard ../inc/*.h)

%.o: %.cpp
	$(CXX) -c -o $@ ${CXXFLAGS} $(CFLAGS) ${COPTS} $<
	@echo "  CXX " $<
../common/%.o: ../common/%.cpp
	$(CXX) -c -o $@ ${CXXFLAGS} $(CFLAGS) ${COPTS} $<
	@echo "  CXX " $<

ifndef CONFIG_OS
CONFIG_OS=unix
endif

LIB_OBJS=$(patsubst %.cpp,%.o,$(wildcard *.cpp))
LIB_OBJS+=$(patsubst %.cpp,%.o,$(wildcard ../common/*.cpp))

ALL= lowi-server

all: ${ALL}

lowi-server: $(LIB_OBJS) $(STATIC_LIBS) 
	${CXX} -o $@ ${CXXFLAGS} ${CFLAGS} -Wall  $^

clean:
	$(MAKE) -C $(LOCAL_PATH_UTILS)/base_util clean
	$(MAKE) -C $(LOCAL_PATH_UTILS)/mq_client clean
	$(MAKE) -C $(LOCAL_PATH_UTILS)/mq_server clean
	$(MAKE) -C wifidriver/romewifidriver clean
	$(MAKE) -C wifidriver/util clean
	rm -f ${ALL}
