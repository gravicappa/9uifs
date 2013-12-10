CC = gcc
O = .o
#CFLAGS = -Wall -O0 -g -pedantic -Wno-long-long
CFLAGS = -Wall -O0 -g -pedantic --std=c99
#CFLAGS = -O2

#CFLAGS = $CFLAGS -pg
#LDFLAGS = -pg

CFLAGS = $CFLAGS `{sdl-config --cflags}
#LDFLAGS = $LDFLAGS -static
LDFLAGS = $LDFLAGS `{sdl-config --libs}

CFLAGS = $CFLAGS -DX_DISPLAY_MISSING
LDFLAGS = $LDFLAGS -lImlib2 -lm

obj = $obj net_unix$O
frontend_obj = raster_imlib$O main_sdl$O
