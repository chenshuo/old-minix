# Makefile for mined

CFLAGS	= -O -wo -D_POSIX_SOURCE
O=o

OBJ = mined1.$O mined2.$O

mined:	$(OBJ)
	@rm -rf mined
	@echo Start linking mined
	@$(CC) -i -o $@ $(OBJ) >/dev/null
	@chmem =64000 mined

$(OBJ):	mined.h

clean:
	@rm -f mined *.o *.s core *.bak
