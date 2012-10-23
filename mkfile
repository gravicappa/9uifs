MKSHELL = rc

name = uifs

LANG = C
CC = gcc
O = o
#CFLAGS = -Wall -O0 -g -pedantic -Wno-long-long
CFLAGS = -Wall -O0 -g 

CFLAGS = $CFLAGS -pg
LDFLAGS = -pg

CFLAGS = $CFLAGS `{sdl-config --cflags}
#LDFLAGS = $LDFLAGS -static
LDFLAGS = $LDFLAGS `{sdl-config --static-libs}

CFLAGS = $CFLAGS -DX_DISPLAY_MISSING
LDFLAGS = $LDFLAGS -lImlib2

exe = $name
run_flags = -s 400x300

<| test -f $target.mk && cat $target.mk || echo

obj = config.$O 9pmsg.$O fs.$O main_sdl.$O util.$O net.$O client.$O \
			fsutil.$O fs.$O 9pdbg.$O surface.c view.c event.$O ctl.$O wm.$O \
			ui.$O uievent.$O prop.$O uiobj_grid.$O uiobj_scroll.$O uiobj_label.$O \
			uiobj_image.$O text.$O font.$O stb_image.$O  images.$O

obj = $obj net_unix.$O
obj = $obj profile.$O

docs = docs/doc.html

all:V: $exe

clean:V:
  rm -f *.$O $exe

docs:V: $docs


$exe.exe: $obj
  $CC $CFLAGS $prereq $LDFLAGS -o $target

$exe: $obj
  $CC $CFLAGS $prereq $LDFLAGS -o $target

test/test_arr: test/test_arr.c util.o
test/test_path: test/test_path.c util.o

%.$O: %.c
  $CC $CFLAGS -c -o $target $stem.c

%: %.c
  $CC $CFLAGS -o $target $prereq

run:V: $exe
  ulimit -c unlimited
  ./$exe $run_flags -d ugc >[2=1] | tee uifs.log

valgrind:V: $exe
  flags=()
  flags=($flags '--read-var-info=yes')
  flags=($flags '--track-origins=yes')
  #flags=($flags '--gen-suppressions=yes')
  flags=($flags '--suppressions=test/xlib.supp')
  flags=($flags '--suppressions=test/imlib.supp')
  if (~ $check '*leak*') flags=($flags '--leak-check=full')
	if not true
	#flags=('--tool=callgrind')
  valgrind $flags ./$exe $run_flags -d ugc >[2=1] | tee $exe.log

%.html: %.md
  sundown <$prereq >$target
