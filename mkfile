MKSHELL = rc

name = uifs

LANG = C

exe = $name
run_flags = -s 400x300
target = unix
<| test -f $target.mk && cat $target.mk || echo

obj = $obj config$O 9pmsg$O fs$O util$O net$O client$O \
      fsutil$O fs$O 9pdbg$O image$O bus$O ctl$O \
      ui$O uiplace$O uievent$O prop$O uiobj_grid$O uiobj_scroll$O \
      uiobj_label$O uiobj_image$O text$O font$O stb_image$O images$O \
      dirty_qtree$O raster$O

obj = $obj profile$O dbg$O

docs = docs/doc.html

all:V: $exe

clean:V:
  rm -f *$O $exe

docs:V: $docs

$exe.exe: $obj $frontend_obj
  $CC $CFLAGS $prereq $LDFLAGS -o $target

$exe: $obj $frontend_obj
  $CC $CFLAGS $prereq $LDFLAGS -o $target

%$O: %.c
  $CC $CFLAGS -c -o $target $stem.c

%: %.c
  $CC $CFLAGS $prereq $LDFLAGS -o $target 

run:V: $exe
  ulimit -c unlimited
  ./$exe $run_flags -d fugc >[2=1] | tee uifs.log

valgrind:V: $exe
  flags=()
  flags=($flags '--read-var-info=yes')
  flags=($flags '--track-origins=yes')
  #flags=($flags '--gen-suppressions=yes')
  flags=($flags '--suppressions=xlib.supp')
  flags=($flags '--suppressions=imlib.supp')
  flags=($flags '--leak-check=full')
  #flags=($flags '--show-reachable=yes')
  valgrind $flags ./$exe $run_flags -d ugc >[2=1] | tee $exe.log

callgrind:V: $exe
  flags=($flags '--read-var-info=yes')
  flags=($flags '--track-origins=yes')
  flags=($flags '--suppressions=xlib.supp')
  flags=($flags '--suppressions=imlib.supp')
  flags=('--tool=callgrind')
  valgrind $flags ./$exe $run_flags -d ugc >[2=1] | tee $exe.log
  out=`{mtime callgrind.out.* | sort -n | awk '{print $2; exit}'}
  callgrind_annotate '--auto='yes $out >callgrind.log

%.html: %.md
  ./docs/toc <$prereq | sundown >$target

gprof:V: $exe gmon.out
  flags=()
  flags=($flags -A)
  flags=($flags -p)
  flags=($flags -q)
  gprof $flags $exe gmon.out >gprof.log

sloc:V: ${obj:%$O=%.c} ${frontend_obj:%$O=%.c}
  wc -l $prereq *.h | sort -n
