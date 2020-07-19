CC ?= gcc

OBJS = ven_cmd_tool.o nl_cmd_wrapper.o

all: cfg80211tool.1

%.o: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c -o $@ $< $(LDFLAGS)

cfg80211tool.1: $(OBJS)
	$(CC) $(LDFLAGS)  -o $@ $(OBJS)

clean:
	rm -rf *.o
