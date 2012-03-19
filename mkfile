name = uifs

CC = gcc
#CFLAGS = -Wall -O0 -g -pedantic -Wno-long-long
CFLAGS = -Wall -O0 -g 

CFLAGS = $CFLAGS `{sdl-config --cflags}
#LDFLAGS = $LDFLAGS -static
LDFLAGS = $LDFLAGS `{sdl-config --static-libs} 

obj = 9pmsg.o fs.o main.o util.o net.o client.o fsutil.o fs.o \
      9pdbg.o surface.c view.c event.o

all:V: $name

clean:V:
	rm *.o $name

$name: $obj
	$CC $CFLAGS $prereq $LDFLAGS -o $target

%.o: %.c
	$CC $CFLAGS -c -o $target $stem.c

test_run:V: $name
	valgrind ./$name -d 10 2>&1 | tee uifs.log
