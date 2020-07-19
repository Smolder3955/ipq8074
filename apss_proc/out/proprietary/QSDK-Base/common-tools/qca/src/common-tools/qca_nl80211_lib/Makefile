CC ?= gcc

OBJS = nl_cmd_wrapper.o

all: libqca_nl80211_wrapper.so

%.o: %.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c -o $@ $< $(LDFLAGS)

libqca_nl80211_wrapper.so: cfg80211_nlwrapper_api.o
	$(CC) -shared -o libqca_nl80211_wrapper.so cfg80211_nlwrapper_api.o

cfg80211_nlwrapper_api.o: cfg80211_nlwrapper_api.c
	$(CC) $(CFLAGS) $(LDFLAGS) -c -o $@ $< $(LDFLAGS)

clean:
	rm -rf *.o
