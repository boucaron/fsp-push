CC ?= cc
# CFLAGS ?= -O2 -Wall -Wextra -I../include 
CFLAGS ?= -g -Wall -Wextra -I../include 

BIN_SEND = fsp-send
BIN_RECV = fsp-recv

OBJS_COMMON = fsp_io.o fsp_opt.o fsp_walk.o fsp_file_processor.o

# Add OpenSSL include & library for SHA256
LDLIBS = -lcrypto

all: $(BIN_SEND) $(BIN_RECV)

$(BIN_SEND): fsp-send.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

$(BIN_RECV): fsp-recv.o $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

# ----------------------------
# Tests
# ----------------------------
TEST_OBJS = tests/test_fsp_dry_run.c tests/demo_fsp_dry_run.c tests/demo_fsp_walk.c tests/demo_fsp_hash.c
TEST_BINS = test_fsp_dry_run demo_fsp_dry_run demo_fsp_walk demo_fsp_hash

test_fsp_dry_run: tests/test_fsp_dry_run.c $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)
demo_fsp_dry_run: tests/demo_fsp_dry_run.c $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)
demo_fsp_walk: tests/demo_fsp_walk.c $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)
demo_fsp_hash: tests/demo_fsp_hash.c $(OBJS_COMMON)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)


.PHONY: tests
tests: $(TEST_BINS)
	@echo "All test binaries built: $(TEST_BINS)"

clean:
	rm -f *.o $(BIN_SEND) $(BIN_RECV) $(TEST_BINS)
