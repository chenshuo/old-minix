.s.o:
	$(CC) $(CFLAGS) -c $<

CC=exec cc
CFLAGS= -c -O -D_MINIX -D_POSIX_SOURCE
S=asin.s atan.s atan2.s ceil.s exp.s fabs.s floor.s fmod.s hugeval.s isnan.s \
  ldexp.s log.s log10.s pow.s sin.s sinh.s sqrt.s tan.s tanh.s 

all:	
	@$(CC) $(CFLAGS) *.c *s


clean:	
	@rm -rf *.o *.bak $S

