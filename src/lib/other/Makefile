CFLAGS= -c -F -LIB -D_MINIX -D_POSIX_SOURCE

$f.s:	$f.c
	@echo ------------------------------ $f ------------------------------
	@cc $(CFLAGS) $f.c


clean:	
	@rm -rf *.s *.bak


all:	
	cc $(CFLAGS) *.c
