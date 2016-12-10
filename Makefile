
CC=gcc
CFLAGS=-gdwarf-3 -Og -Wall
LDFLAGS=

TSMERGE_BIN = tsmerge
TSMERGE_SRCS = main.c \
                ts.c \
                timing.c \
                merger.c \
                merger_rx.c \
                merger_tx.c \
                merger_tx_socket.c \
                merger_rx_buffer.c \
                merger_rx_feed.c

TSMERGE_LIBS = -lpthread

TSPUSH_BIN = tspush
TSPUSH_SRCS = push.c \
                ts.c

all: tspush tsmerge

tsmerge:
	$(CC) $(CFLAGS) $(TSMERGE_SRCS) -o $(TSMERGE_BIN) $(LDFLAGS) $(TSMERGE_LIBS)

tspush:
	$(CC) $(CFLAGS) $(TSPUSH_SRCS) -o $(TSPUSH_BIN) $(LDFLAGS)

clean:
	rm -fv *.o $(TSMERGE_BIN) $(TSPUSH_BIN)

