.s.o:
	$(CC) $(CFLAGS) $<

CC=exec cc
CFLAGS= -c -O -D_MINIX -D_POSIX_SOURCE


all:	
	@$(CC) $(CFLAGS) *.c *.s


clean:	
	@rm -rf *.o *.bak stb.s

