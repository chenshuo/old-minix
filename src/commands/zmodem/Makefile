# Makefile for zmodem

all: rz sz

CFLAGS= -m -DV7 -D_MINIX -D_POSIX_SOURCE -DLCASE=0 -O
O=o

rz: rz.c rbsb.c zm.c zmodem.h
	@rm -rf rz
	@cc $(CFLAGS) -o rz rz.c
	@chmem =10000 rz

sz: sz.c rbsb.c zm.c zmodem.h
	@rm -rf sz
	@cc $(CFLAGS) -o sz sz.c
	@chmem =10000 sz

clean:	
	@rm -f *.bak *.o *.s core rz sz
