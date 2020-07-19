INCLUDE_DIR += -I./inc -I./common/inc -I./smem_log  -I./qcci/inc
QMI_SRC_DIR := common/src
QMI_CCI_DIR := qcci/src
SMEM_LOG_DIR := smem_log
LIB := -lpthread -shared
FLAGS = -fPIC -g
LDFLAGS += $(TARGET_LDFLAGS)
EXTRA_CFLAGS = $(TARGET_CPPFLAGS) $(TARGET_CFLAGS) -fstack-protector-all


libqmicommon := $(QMI_SRC_DIR)/common_v01.c
libqmicci := $(QMI_CCI_DIR)/qmi_cci_common.c \
	$(QMI_CCI_DIR)/qmi_cci_target.c \
	$(QMI_CCI_DIR)/qmi_cci_xport_ipc_router.c \
	encdec/qmi_idl_lib.c encdec/qmi_idl_accessor.c \
	$(SMEM_LOG_DIR)/smem_log.c \

all:
	$(CC) -o libqmi_common_so.so $(libqmicommon) $(FLAGS) $(INCLUDE_DIR) $(LIB) -znow -zrelro
	mkdir -p lib
	mv libqmi_common_so.so lib/
	$(CC) -o libqmi_cci.so $(libqmicci) $(FLAGS) $(LDFLAGS) $(EXTRA_CFLAGS) $(INCLUDE_DIR) $(LIB) -Llibqmi_common_so.so
	mv libqmi_cci.so lib/

clean:
	rm -rf lib/
	rm -rf *.o
