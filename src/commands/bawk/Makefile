# Makefile for bawk - pattern matching language

OPSYS   = MINIX
CFLAGS  = -D$(OPSYS) -wo
O=o

OBJ     = bawk.$O bawkact.$O bawksym.$O bawkpat.$O bawkdo.$O

bawk:       $(OBJ)
	    @rm -rf bawk
	    @echo Start linking bawk
	    @cc -i -o bawk $(OBJ) >/dev/null
	    @chmem =20000 bawk

bawk.$O:     bawk.h bawk.c
bawkact.$O:  bawk.h bawkact.c
bawkpat.$O:  bawk.h bawkpat.c
bawksym.$O:  bawk.h bawksym.c
bawkdo.$O:   bawk.h bawkdo.c

clean:	
	@rm -f *.bak *.o *.s core bawk
