INCLUDE_DIR += -I./ -I include -I src
LDFLAGS += -Llib/ -lpthread -lqmi_common_so -lqmi_cci $(TARGET_LDFLAGS) -pie
FLAGS = -D _GNU_SOURCE -DQSDK_BUILD
EXTRA_CFLAGS = $(TARGET_CFLAGS) $(TARGET_CPPFLAGS) -fstack-protector-all -fpie
LIB := -lpthread

time-services-obj := time_daemon_qmi.c time_service_v01.c wlan_time_service_v01.c

all:
	$(CC) -o time-services-app  $(time-services-obj) $(FLAGS) $(EXTRA_CFLAGS) $(LDFLAGS) -I $(INCLUDE_DIR) -I$(STAGING_DIR)/usr/include
	mkdir -p bin
	cp time-services-app bin/

clean:
	rm -rf bin/
	rm -rf *.o
