# Makefile for make!

CFLAGS = -Dunix -DMINIX -F -T.

OBJS	=	check.s input.s macro.s main.s \
		make.s reader.s rules.s

make:		$(OBJS)
	cc -o make $(OBJS)

check.s:	h.h check.c
	$(CC) -S $(CFLAGS) check.c

input.s:	h.h input.c
	$(CC) -S $(CFLAGS) input.c

macro.s:	h.h macro.c
	$(CC) -S $(CFLAGS) macro.c

main.s:		h.h main.c
	$(CC) -S $(CFLAGS) main.c

make.s:		h.h make.c
	$(CC) -S $(CFLAGS) make.c

reader.s:	h.h reader.c
	$(CC) -S $(CFLAGS) reader.c

rules.s:	h.h rules.c
	$(CC) -S $(CFLAGS) rules.c
