
CC := $(TOOLPREFIX)gcc
TARGET_TYPE ?= AR900B
TARGET_VERS ?= v1

ifeq ($(TARGET_TYPE), AR900B)
CFLAGS += -DAR900B -DWLAN_HEADERS
endif

CFLAGS += -fpie
LDFLAGS += -pie

all:
	$(CC) -g3 -Wall  \
		-I include/ \
		-I $(DL_DIR)/include/$(TARGET_TYPE)/$(TARGET_VERS)/ \
		-I $(DL_DIR)/include/$(TARGET_TYPE)/extra/ \
		-I $(DL_DIR)/include/$(TARGET_TYPE)/hw/ \
		-I $(DL_DIR)/include/$(TARGET_TYPE)/extra/hw/ \
		-I $(DL_DIR)/include/ \
		$(CFLAGS) \
		$(LDFLAGS) \
		athdiag.c Qc98xxEepromStructGet.c Qc98xxDevice.c Qc98xxEepromStruct.c -o athdiag


clean:
	rm -f athdiag
