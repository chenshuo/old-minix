# Makefile for rlogind.

CFLAGS	= $(OPT) -D_MINIX
LDFLAGS	=

all:	in.rld

OBJ=	in.rld.o setup.o

in.rld:	$(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)
	install -S 4kw $@

install:	/usr/bin/in.rld

/usr/bin/in.rld:	in.rld
	install -c $? $@

clean:
	rm -f *.o in.rld core a.out

# Dependencies.
$(OBJ):		in.rld.h
