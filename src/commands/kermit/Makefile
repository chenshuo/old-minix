# Makefile for kermit

# CKUMNX.MAK, Version 2.11, 29 January 1988
#
# -- Makefile to build C-Kermit for Minix.  This is a separate file
# because Minix uses .o instead of .o for its object files. This
# means that not one line of the file is the same as the standard
# Unix makefile, so it seems silly to clutter the common file.

###########################################################################
#
#  Compile and Link variables:
#
CC=exec cc
LNKFLAGS=-i
CFLAGS=-DV7 -DMINIX -O -wo
O=s
#
###########################################################################
#
# Dependencies Section:
#

kermit: ckcmai.$O ckucmd.$O ckuusr.$O ckuus2.$O ckuus3.$O ckcpro.$O ckcfns.$O \
		 ckcfn2.$O ckucon.$O ckutio.$O ckufio.$O ckudia.$O ckuscr.$O
	@rm -rf kermit
	@echo Start linking kermit
	@$(CC) $(LNKFLAGS) -o kermit ckcmai.$O ckutio.$O \
		 ckufio.$O ckcfns.$O \
		 ckcfn2.$O ckcpro.$O ckucmd.$O ckuus2.$O ckuus3.$O ckuusr.$O \
		 ckucon.$O ckudia.$O ckuscr.$O >/dev/null
	@chmem =65000 kermit

ckcmai.$O: ckcmai.c ckcker.h ckcdeb.h ckcsym.h
ckuusr.$O: ckuusr.c ckucmd.h ckcker.h ckuusr.h ckcdeb.h
ckuus2.$O: ckuus2.c ckucmd.h ckcker.h ckuusr.h ckcdeb.h
ckuus3.$O: ckuus3.c ckucmd.h ckcker.h ckuusr.h ckcdeb.h
ckucmd.$O: ckucmd.c ckucmd.h ckcdeb.h
ckcpro.$O: ckcpro.c ckcker.h ckcdeb.h

# I'm using the distributed copy of ckcpro.c.  Unfortunately
# wart won't compile.  If you need to change ckcpro.w, you'll have
# to find a way to reduce the number of strings in ckwart.c
#
# ckcpro.c: ckcpro.w wart
#	./wart ckcpro.w ckcpro.c

ckcfns.$O: ckcfns.c ckcker.h ckcdeb.h ckcsym.h
ckcfn2.$O: ckcfn2.c ckcker.h ckcdeb.h ckcsym.h
ckufio.$O: ckufio.c ckcker.h ckcdeb.h
ckutio.$O: ckutio.c ckcdeb.h
ckucon.$O: ckucon.c ckcker.h ckcdeb.h

wart: ckwart.$O
	$(CC) $(LNKFLAGS) -o wart ckwart.$O

ckwart.$O: ckwart.c
ckudia.$O: ckudia.c ckcker.h ckcdeb.h ckucmd.h
ckuscr.$O: ckuscr.c ckcker.h ckcdeb.h

#Clean up intermediate and object files
clean:
	@rm -f *.o *.s *.bak core kermit

