MKSHELL = rc

name = uifs

LANG = C
CC = gcc
#CFLAGS = -Wall -O0 -g -pedantic -Wno-long-long
CFLAGS = -Wall -O0 -g

CFLAGS = $CFLAGS `{sdl-config --cflags}
#LDFLAGS = $LDFLAGS -static
LDFLAGS = $LDFLAGS `{sdl-config --static-libs}

CFLAGS = $CFLAGS -DX_DISPLAY_MISSING
LDFLAGS = $LDFLAGS -lImlib2

obj = config.o 9pmsg.o fs.o main_sdl.o util.o net.o client.o fsutil.o fs.o \
      9pdbg.o surface.c view.c event.o ctl.o wm.o ui.o uievent.o prop.o \
      uiobj_grid.o uiobj_scroll.o uiobj_label.o

docs = docs/doc.html

all:V: $name

clean:V:
  rm -f *.o $name

docs:V: $docs

$name: $obj
  $CC $CFLAGS $prereq $LDFLAGS -o $target

test/test_arr: test/test_arr.c util.o
test/test_path: test/test_path.c util.o

%.o: %.c
  $CC $CFLAGS -c -o $target $stem.c

%: %.c
  $CC $CFLAGS -o $target $prereq

run:V: $name
  ulimit -c unlimited
  ./$name -d uc >[2=1] | tee uifs.log

valgrind:V: $name
  flags=()
  flags=($flags '--read-var-info=yes')
  flags=($flags '--track-origins=yes')
  if (~ $check '*leak*') flags=($flags '--leak-check=full')
	if not true
  valgrind '--suppressions=test/xlib.supp' $flags ./$name -d ugc >[2=1] \
	| tee $name.log

%.html: %.md
  sundown <$prereq >$target
