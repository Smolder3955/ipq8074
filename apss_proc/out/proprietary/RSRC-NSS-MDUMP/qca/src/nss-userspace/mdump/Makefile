EXEC = mdump

all: $(EXEC)

$(EXEC): $(EXEC).c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $@.c

clean:
	rm -f $(EXEC) *.elf *.gdb *.o

