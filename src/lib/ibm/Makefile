CFLAGS= -c -F -LIB -D_MINIX -D_POSIX_SOURCE
OBJ=portio.s peekpoke.s
$f.s:	$f.c
	@echo ------------------------------ $f ------------------------------
	@cc $(CFLAGS) $f.c


clean:	
	@rm -rf $(OBJ) *.bak


all:	
	@rm -f $(OBJ)
	@ls *.s >tmp.1
	@sed <tmp.1 >tmp.2 '/.*/s//& >\/tmp\/&/;/^/s//libpack </'
	@sh tmp.2
	@rm tmp.?
	@ls *.c >tmp.1
	@sed <tmp.1 >tmp.2 '/\.c/s///;/^/s//make f=/'
	@rm -f tmp.1 $(OBJ) *.bak
	@sh tmp.2
	@rm tmp.?
