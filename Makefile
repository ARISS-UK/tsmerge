

CC = gcc
CFLAGS = -Og -Wall
CFLAGS += -D BUILD_VERSION="\"$(shell git describe --dirty --always)\""	\
		-D BUILD_DATE="\"$(shell date '+%Y-%m-%d %H:%M:%S')\""

## tsmerge

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

## tspush

TSPUSH_BIN = tspush
TSPUSH_SRCS = push.c \
                ts.c

## tsinfo

TSINFO_BIN = tsinfo
TSINFO_SRCS = info.c \
                ts.c
## tssim

TSSIM_BIN = tssim
TSSIM_SRCS = sim.c \
                ts.c

### Targets        

TARGETS = $(TSMERGE_BIN) $(TSPUSH_BIN) $(TSINFO_BIN) $(TSSIM_BIN)

all: $(TARGETS)

tsmerge:
	$(CC) $(CFLAGS) $(TSMERGE_SRCS) -o $(TSMERGE_BIN) $(TSMERGE_LIBS)

tspush:
	$(CC) $(CFLAGS) $(TSPUSH_SRCS) -o $(TSPUSH_BIN)

tsinfo:
	$(CC) $(CFLAGS) $(TSINFO_SRCS) -o $(TSINFO_BIN)

tssim:
	$(CC) $(CFLAGS) $(TSSIM_SRCS) -o $(TSSIM_BIN)

clean:
	rm -fv *.o $(TARGETS)

