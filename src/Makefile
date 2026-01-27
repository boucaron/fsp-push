CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -I../include

BIN_SEND = fsp-send
BIN_RECV = fsp-recv

OBJS_COMMON = fsp_io.o fsp_opt.o fsp_fs.o fsp_walk.o

all: $(BIN_SEND) $(BIN_RECV)

$(BIN_SEND): fsp-send.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

$(BIN_RECV): fsp-recv.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f *.o $(BIN_SEND) $(BIN_RECV)
