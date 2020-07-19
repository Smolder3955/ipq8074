CFLAGS += -g -DDEBUG -Ishared -DLINUX_X86 -Os -fPIC
LDLIBS= -lpthread -lrt -lm -rdynamic -ldl

# SIGEV_NOTIFY_SIGNAL is used to enable SIGEV_SIGNAL and disable SIGEV_THREAD
# USER should define SIGEV_NOTIFY_SIGNAL=y to enable SIGEV_SIGNAL
# or other value to disable SIGEV_SIGNAL
ifeq ($(SIGEV_NOTIFY_SIGNAL), y)
CFLAGS += -DSIGEV_NOTIFY_SIGNAL
endif


OBJS=   Qcmbr.o \
	diag_pkt_handler.o \
	tlvCmd_if_Qdart.o \
	Socket.o

.PHONY: all clean

Qcmbr.o : Qcmbr.c Qcmbr.h
	$(CC) -c $(CFLAGS) Qcmbr.c
diag_pkt_handler.o : diag_pkt_handler.c
	$(CC) -c $(CFLAGS) diag_pkt_handler.c

ifeq ($(USE_NETLINK),1)
tlvCmd_if_Qdart.o : tlvCmd_if_Qdart.c
	$(CC) -DUSE_NETLINK=1 -c $(CFLAGS) tlvCmd_if_Qdart.c
else
tlvCmd_if_Qdart.o : tlvCmd_if_Qdart.c
	$(CC) -c $(CFLAGS) tlvCmd_if_Qdart.c
endif

Socket.o  : ./shared/Socket.c ./shared/Socket.h
	$(CC) -c $(CFLAGS) ./shared/Socket.c

ifeq ($(USE_NETLINK),1)
q_os_nl.o : q_os_nl.c
	$(CC) -c $(CFLAGS) q_os_nl.c
OBJS += q_os_nl.o
else
q_os_if.o : q_os_if.c
	$(CC) -c $(CFLAGS) q_os_if.c
OBJS += q_os_if.o
endif

Qcmbr: $(OBJS)
	$(CC) $(OBJS) $(LDLIBS) $(LDFLAGS) -o Qcmbr

clean:
	-$(RM) *.o
	-$(RM) Qcmbr

