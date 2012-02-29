name = devyat

CC = gcc
CFLAGS = -Wall -O0 -g -ansi -pedantic

CFLAGS = $CFLAGS `{sdl-config --cflags}
#LDFLAGS = $LDFLAGS -static
LDFLAGS = $LDFLAGS `{sdl-config --static-libs} 

obj = 9pmsg.o 9pio.o fs.o main.o util.o net.o client.o

all:V: $name

clean:V:
	rm *.o $name

msg:V:
	awk -f msg.awk < msg.defs

$name: $obj
	$CC $CFLAGS $prereq $LDFLAGS -o $target

%.o: %.c
	$CC $CFLAGS -c -o $target $stem.c
