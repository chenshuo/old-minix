# Makefile for the kernel image.
#
u=/usr
CC=	exec cc
CFLAGS=	-O -D_MINIX -D_POSIX_SOURCE
LDFLAGS=
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

init:	init.c
	$(CC) $(LDFLAGS) $(CFLAGS) init.c -o $@
	install -S 192w $@

bootable:
	exec su root mkboot bootable

hdinstall hdtest fdboot:	programs
	exec su root mkboot $@

all install clean::
	cd ../kernel && $(MAKE) $@
	cd ../mm && $(MAKE) $@
	cd ../fs && $(MAKE) $@
	cd ../inet && $(MAKE) $@

clean::
	rm -f *.bak init image
