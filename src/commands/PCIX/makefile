# To make  'cp', type:   make f=cp
# To make  'ls', type:   make f=ls
# To make  'cat', type:   make f=cat
# Get the idea?

# It is absolutely essential that no routines are extracted from -lc because
# they may contain system calls.  Instead, find out which library routines are
# needed (e.g., lmul.o, ldiv.o, lrem.o, divsub.o, etc) and extract these
# from /lib/libc.a and insert them in the MINIX library.

l=../lib
i=../include
CFLAGS=-O -I../include

file:	$l/libc.a $f.o
	@ld -s -o bin/$f $l/crtso.o $f.o $l/libc.a $l/end.o
	@chmem =2048 bin/$f >/dev/null
	@echo "$f done ."

mined:	mined1.o mined2.o $i/mined.h
	@ld -s -i -o bin/mined $l/crtso.o mined1.o mined2.o $l/libc.a $l/end.o

make:	make.o
	@ld -s -i -o bin/make $l/crtso.o make.o $l/libc.a $l/end.o
	@chmem =20000 bin/make

shobj = sh1.o sh2.o sh3.o sh4.o sh5.o
sh:	$(shobj) $i/sh.h
	@ld -s -i -o bin/sh $l/crtso.o $(shobj) $l/libc.a $l/end.o
	@chmem =8000 bin/sh

