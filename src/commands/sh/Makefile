# Makefile for sh

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE -m -wo
O=o

OBJ	= sh1.$O sh2.$O sh3.$O sh4.$O sh5.$O sh6.$O

sh:	$(OBJ)
	@rm -rf sh
	@echo Start linking sh
	@cc -o $@ $(OBJ) >/dev/null
	@chmem =21000 sh

$(OBJ):	sh.h

clean:
	@rm -f sh *.o *.s *.bak core
