# Makefile for advent

CFLAGS	= -m -D_POSIX_SOURCE -D_MINIX
O=o

OBJS   = advent.$O database.$O english.$O itverb.$O turn.$O verb.$O

advent:	$(OBJS)
	cc -i -o advent $(OBJS)

advent0: advent0.$O
	cc -o advent0 advent0.$O
 
advent.$O: advent.h advword.h advtext.h advdef.h
database.$O: advent.h advdec.h advcave.h
english.$O itverb.$O turn.$O verb.$O: advent.h advdec.h

clean:	
	@rm -rf *.o *.s *.bak core advent
