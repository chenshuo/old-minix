# Makefile for shutdown / halt / wall.

CFLAGS=$(OPT) -D_MINIX -D_POSIX_SOURCE
LDFLAGS=-i

OBJS=	halt.o wall.o shutdown.o sh_wall.o log.o
PROGRAMS=	wall shutdown halt tinyhalt
MANUALS=	shutdown.8 halt.8 reboot.2 reboot.8
SRC=	sh_wall.c shutdown.c wall.c halt.c log.c

all:	$(PROGRAMS)

wall:	wall.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o wall wall.c
	install -S 4kw $@

shutdown:	shutdown.o sh_wall.o log.o
	$(CC) $(LDFLAGS) -o shutdown shutdown.o sh_wall.o log.o
	install -S 4kw $@

halt:	halt.o log.o
	$(CC) $(LDFLAGS) -o halt halt.o log.o
	install -S 4kw $@

tinyhalt:	tinyhalt.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $?
	install -S 4kw $@

install:	/usr/bin/halt /usr/bin/reboot /usr/bin/wall \
		/usr/bin/shutdown /bin/halt /bin/reboot

/usr/bin/halt:	halt
	install -cs -o root -g operator -m 744 $? $@

/usr/bin/reboot:	/usr/bin/halt
	install -l $? $@

/usr/bin/wall:	wall
	install -cs -o bin $? $@

/usr/bin/shutdown:	shutdown
	install -cs -o root -g operator -m 4754 $? $@

/bin/halt:	tinyhalt
	install -cs -o root -g operator -m 744 $? $@

/bin/reboot:	/bin/halt
	install -l $? $@

clean:
	rm -f $(OBJS) core $(PROGRAMS)
