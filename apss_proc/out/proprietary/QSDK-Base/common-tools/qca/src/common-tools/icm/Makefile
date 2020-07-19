#
# Makes icm apps
#

ALL+= icm
override CFLAGS += -fpie
override LDFLAGS += -pie

all: ${ALL}

SOURCE_FILES := icm.c icm_socket.c icm_test.c icm_scan.c icm_utils.c icm_selector.c icm_cmds.c icm_wal.c icm_wal_ioctl.c icm_wal_rtrcfg.c icm_wal_mblcfg.c driver/events.c driver/driver_nl80211.c

ifeq ($(strip $(WLAN_SPECTRAL_ENABLE)),1)
	override CFLAGS += -DWLAN_SPECTRAL_ENABLE
	SOURCE_FILES += icm_spectral.c
endif

icm: $(SOURCE_FILES)
	${CC} -o icm ${CFLAGS} ${LDFLAGS} -lpthread $(SOURCE_FILES) $(STAGING_DIR)/usr/lib/libath_classifier.a -lm

clean:
	@rm -rf icm
