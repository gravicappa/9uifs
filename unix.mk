CC = gcc
O = o
#CFLAGS = -Wall -O0 -g -pedantic -Wno-long-long
CFLAGS = -Wall -O0 -g

#CFLAGS = $CFLAGS -pg
#LDFLAGS = -pg

CFLAGS = $CFLAGS `{sdl-config --cflags}
#LDFLAGS = $LDFLAGS -static
LDFLAGS = $LDFLAGS `{sdl-config --static-libs}

CFLAGS = $CFLAGS -DX_DISPLAY_MISSING
LDFLAGS = $LDFLAGS -lImlib2

obj = $obj net_unix.$O