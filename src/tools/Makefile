# Makefile for the kernel image.
#
u=/usr
CC=	exec cc
LD=	$(CC) -.o
CFLAGS=	-O -D_MINIX -D_POSIX_SOURCE
LDFLAGS=
HEAD=	$u/lib/head.o
MDEC=	/usr/mdec
MAKE=	exec make -$(MAKEFLAGS)
inet=
PROGRAMS=	../kernel/kernel ../mm/mm ../fs/fs $(inet) init

programs:
	cd ../kernel && $(MAKE)
	cd ../mm && $(MAKE)
	cd ../fs && $(MAKE)
	@if [ `exec ./tell_config ENABLE_NETWORKING` = 0 ]; then \
		$(MAKE) image; \
	else \
		echo "cd ../inet && $(MAKE)" && \
		(cd ../inet && $(MAKE)) && \
		$(MAKE) image inet=../inet/inet; \
	fi

image:	$(PROGRAMS)
	installboot -image $@ $(PROGRAMS)

init:	$(HEAD) init.c
	$(LD) $(LDFLAGS) $(CFLAGS) $(HEAD) init.c -o init

bootable:
	exec ./mkboot bootable

hdinstall hdtest fdboot:	programs
	exec ./mkboot $@

all install clean::
	cd ../kernel && $(MAKE) $@
	cd ../mm && $(MAKE) $@
	cd ../fs && $(MAKE) $@
	cd ../inet && $(MAKE) $@

clean::
	rm -f *.bak init image
