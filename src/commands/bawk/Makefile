# Makefile for bawk - pattern matching language

OPSYS   = MINIX
CFLAGS  = -D$(OPSYS)

all:	bawk

OBJ     = bawk.o bawkact.o bawksym.o bawkpat.o bawkdo.o

bawk:       $(OBJ)
	    cc -i -o bawk $(OBJ)
	    install -S 10kw bawk

install:	/usr/bin/bawk /usr/bin/awk

/usr/bin/bawk:	bawk
	install -cs -o bin bawk $@

/usr/bin/awk:	/usr/bin/bawk
	install -l /usr/bin/bawk $@

bawk.o:     bawk.h bawk.c
bawkact.o:  bawk.h bawkact.c
bawkpat.o:  bawk.h bawkpat.c
bawksym.o:  bawk.h bawksym.c
bawkdo.o:   bawk.h bawkdo.c

clean:	
	rm -f *.bak *.o core bawk
