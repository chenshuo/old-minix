# Makefile for patch

CFLAGS= -O -DVOIDSIG -wo -D_MINIX -D_POSIX_SOURCE -DSMALL -m
O=o

OBJ = patch.$O pch.$O inp.$O util.$O version.$O

patch: $(OBJ)
	@rm -rf patch
	@echo Start linking patch
	@cc -i -s -o patch $(OBJ) >/dev/null

patch.$O:	config.h common.h patch.c inp.h pch.h util.h version.h
pch.$O:		config.h common.h pch.c pch.h util.h
inp.$O:		config.h common.h inp.c inp.h util.h
util.$O:	config.h common.h util.c util.h
version.$O:	config.h common.h version.c version.h patchlevel.h util.h


clean:	
	@rm -f *.bak *.o *.s core patch
