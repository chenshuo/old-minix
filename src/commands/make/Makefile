# Makefile for make

CFLAGS = -O -Dunix -DMINIXPC -Dprintf=printk -Dfprintf=fprintk
O=o

OBJ =	check.$O input.$O macro.$O main.$O make.$O reader.$O rules.$O

make :	$(OBJ)
	@rm -rf make
	@echo Start linking make
	@$(CC) -o make $(OBJ) >/dev/null
	@chmem =35000 make
 
$(OBJ): h.h

clean:
	@rm -f *.o *.s *.bak core make
