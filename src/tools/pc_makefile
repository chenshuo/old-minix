l=/usr/lib
CFLAGS = -F -T.

all:
	make init
	make bootblok
	make build

init:	$l/libc.a init.s $l/head.s
	asld -o init  $l/head.s init.s $l/libc.a  $l/end.s
	@echo init done.

# bootblok.s is the source of the MINIX boot block.  The bootblock is the
# first 512 bytes on the image file and on the boot diskette.  When bootblok.s
# is assembled, it generates a short binary file (less than 400 bytes) that
# must be stripped of its header and copied to the file bootblok.  The dd
# command below does this.  If everything has been done right, the bootblok
# file should begin with the following 8 words, in hex:
# c0b8 8e07 33d8 b8f6 2fe0 c08e ff33 00b9
# The exact procedure for going from bootblok.s to the binary file stripped of
# its header is somewhat operating system dependent.  Some assemblers make
# an object (.s) file; others make an a.out file directly. If your boot 
# diskette does not start out by printing 'Booting MINIX 1.0' the problem is
# probably that you have not made a good boot block.
bootblok:	bootblok.s
	@asld  bootblok.s 
	@dd if=a.out of=bootblok bs=16w skip=1 count=16 2>/dev/null
	@rm a.out
	@echo bootblok done.

build:	build.s
	cc -o build build.s
	@echo build done.

fsck:	fsck.s fsck1.s
	@echo "Start linking fsck. "
	asld -o fsck fsck1.s fsck.s $l/libc.a $l/end.s
	@echo fsck done.
fsck.s: fsck.c
	cc -c -Di8088 -DSTANDALONE -F fsck.c

# 'make image'  combines the bootblock, kernel, memory manager, file 
# system, init and fsck into a single file, called image.  Each of these pieces
# appears in the image file just as the original does on the disk, except that
# the header at the front is removed, and each piece is padded out to an
# integral multiple of 16 bytes.  Build also prints a message telling how big
# the various pieces (except fsck) are.
#
# 'make net' does the same thing, only with the networking code from the
# 'amoeba directory included
#
image:	build bootblok 
	@getlf "Insert blank diskette in drive 0 and hit return"
	@build bootblok ../kernel/kernel ../mm/mm ../fs/fs init fsck image
	@cp image /dev/fd0

net:	build bootblok 
	@getlf "Insert blank diskette in drive 0 and hit return"
	@build bootblok ../amoeba/kernel/kernel ../amoeba/mm/mm \
		../amoeba/fs/fs init fsck /dev/fd0

