

CC=gcc
CFLAGS=-gdwarf-3 -Og -Wall
LDFLAGS=

TSMERGE_BIN = tsmerge
TSMERGE_SRCS = main.c \
                ts.c \
                timing.c \
                merger.c \
                merger_stats.c \
                merger_rx_feed.c \
                merger_rx_socket.c \
                merger_tx_feed.c \
                merger_tx_socket.c \
                merger_file_feed.c \
                merger_rx_buffer.c

TSMERGE_LIBS = -lpthread

TSPUSH_BIN = tspush
TSPUSH_SRCS = push.c \
                ts.c

TSINFO_BIN = tsinfo
TSINFO_SRCS = info.c \
                ts.c

TSSIM_BIN = tssim
TSSIM_SRCS = sim.c \
                ts.c
                

TARGETS = $(TSMERGE_BIN) $(TSPUSH_BIN) $(TSINFO_BIN) $(TSSIM_BIN)

all: $(TARGETS)

tsmerge:
	$(CC) $(CFLAGS) $(TSMERGE_SRCS) -o $(TSMERGE_BIN) $(LDFLAGS) $(TSMERGE_LIBS)

tspush:
	$(CC) $(CFLAGS) $(TSPUSH_SRCS) -o $(TSPUSH_BIN) $(LDFLAGS)

tsinfo:
	$(CC) $(CFLAGS) $(TSINFO_SRCS) -o $(TSINFO_BIN) $(LDFLAGS)

tssim:
	$(CC) $(CFLAGS) $(TSSIM_SRCS) -o $(TSSIM_BIN) $(LDFLAGS)

clean:
	rm -fv *.o $(TARGETS)

