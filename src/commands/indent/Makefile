# Makefile for indent

CFLAGS= -c -O -D_MINIX -D_POSIX_SOURCE
O=o

OBJ = args.$O comment.$O lexi.$O indent.$O parse.$O io.$O

indent: ${OBJ}
	@rm -rf indent
	@echo Start linking indent
	@$(CC) -o indent -i ${OBJ} >/dev/null
	@chmem =60000 indent

$(OBJ): globs.h codes.h

clean:
	@rm -f *.bak *.o *.s core indent
