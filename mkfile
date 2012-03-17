name = uifs

CC = gcc
CFLAGS = -Wall -O0 -g -pedantic -Wno-long-long

CFLAGS = $CFLAGS `{sdl-config --cflags}
#LDFLAGS = $LDFLAGS -static
LDFLAGS = $LDFLAGS `{sdl-config --static-libs} 

obj = 9pmsg.o 9pio.o fs.o main.o util.o net.o client.o fsutil.o fs.o \
      9pdbg.o surface.c view.c

all:V: $name

clean:V:
	rm *.o $name

msg:V:
	awk -f msg.awk < msg.defs

$name: $obj
	$CC $CFLAGS $prereq $LDFLAGS -o $target

%.o: %.c
	$CC $CFLAGS -c -o $target $stem.c
