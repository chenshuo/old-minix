# Makefile for ic

CFLAGS	= -O -D_POSIX_SOURCE -D_MINIX
O=o

OBJ   = ic.$O ic_input.$O ic_output.$O 

ic:	$(OBJ)
	@rm -rf ic
	@echo Start linking ic
	@cc  $(OBJ) -o ic >/dev/null
	@chmem =8192 ic

$(ICOBJ): ic.h

clean:
	@rm -f *.o *.s ic *.bak core
