CC = gcc
CFLAGS = -Wall -Wextra -O0 -g -ansi -pedantic

all:V:

msg:V:
	awk -f msg.awk < msg.defs

%.o: %.c
	$CC $CFLAGS -c -o $target $stem.c
