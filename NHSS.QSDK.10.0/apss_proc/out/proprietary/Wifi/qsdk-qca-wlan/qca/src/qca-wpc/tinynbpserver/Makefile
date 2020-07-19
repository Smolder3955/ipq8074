GUI_LIBS = `pkg-config  --libs --cflags gtk+-2.0 gthread-2.0` `pkg-config --libs --cflags libconfig`

OBJS = main.o debug/dump.o \
		lib/lib.o \
		connections/connections.o 

IFFLAGS = -I ./include  -I ../../qca-wifi/os/linux/include

all:
	make build DEBUG=-g
	
build:
	make -C lib/ all DEBUG=$(DEBUG)
	make -C connections/ all DEBUG=$(DEBUG)
	make -C debug/ all DEBUG=$(DEBUG)
	sh genversion.sh
	gcc $(DEBUG) -c $(IFFLAGS) main.c
	gcc -o nbpserver $(OBJS) $(GUI_LIBS)   -lm -lpthread

WPC_OBJS = wpc_dbg_log.o 

wpc_debug:
	gcc -g -c $(IFFLAGS) wpc_dbg_log.c
	gcc -o wpc_debug $(WPC_OBJS)

wpc_debug_clean:
	rm -f wpc_dbg_log.o wpc_debug

clean:
	rm -f nbpserver
	rm -f *.o
	make -C lib/  clean
	make -C connections/ clean
	make -C debug/ clean 

updatecs:
	cscope -R
	ctags -R

.PHONY : all  build
