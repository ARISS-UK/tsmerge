

CC = gcc
CFLAGS = -Og -Wall
CFLAGS += -D BUILD_VERSION="\"$(shell git describe --dirty --always)\""	\
		-D BUILD_DATE="\"$(shell date '+%Y-%m-%d %H:%M:%S')\""

## tsmerge

TSMERGE_BIN = tsmerge
TSMERGE_SRCS = merge.c \
                ts/ts.c \
                timing/timing.c \
                merger/merger.c \
                merger/stats.c \
                merger/input_socket.c \
                merger/input_feed.c \
		merger/input_buffer.c \
		merger/output_socket.c \
		merger/output_feed.c \
		merger/output_log.c

TSMERGE_LIBS = -lpthread

## tspush

TSPUSH_BIN = tspush
TSPUSH_SRCS = push.c \
                ts/ts.c

## tsinfo

TSINFO_BIN = tsinfo
TSINFO_SRCS = info.c \
                ts/ts.c
## tssim

TSSIM_BIN = tssim
TSSIM_SRCS = sim.c \
                ts/ts.c

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

