# Makefile for nroff

CFLAGS=-D_MINIX -D_POSIX_SOURCE -F
O=o
OBJS = main.$O command.$O text.$O io.$O macros.$O strings.$O escape.$O low.$O

nroff:	$(OBJS) nroff.h
	@rm -rf mroff
	@echo Start linking nroff
	@cc -i -o nroff $(OBJS) >/dev/null
	@chmem =65000 nroff

clean:	
	@rm -f *.bak *.o *.s core nroff
