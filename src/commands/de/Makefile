# Makefile for de

CC	= cc
CFLAGS	= -m -O -D_MINIX -D_POSIX_SOURCE -D_POSIX_1_SOURCE=2
O=o

OBJS	= de.$O de_stdin.$O de_stdout.$O de_diskio.$O de_recover.$O

de:	$(OBJS)
	@rm -rf de
	@echo Start linking de
	@$(CC) -i $(OBJS) -o de >/dev/null
	@chmem =8192 de

$(OBJS): de.h

clean:
	@rm -f *.bak *.o *.s de
