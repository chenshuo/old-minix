# Makefile for elle

O=o
CFLAGS = -c -O -DIGN_JOB_CONTROL -D_POSIX_SOURCE -m -wa -I$(INC)
CC=exec cc
INC=/include

OBJ = eemain.$O eecmds.$O eesite.$O eevini.$O eedisp.$O eeterm.$O eeerr.$O  \
      eeques.$O eebuff.$O eefile.$O eefed.$O eeedit.$O eebit.$O eef1.$O \
      eef2.$O eefd.$O eehelp.$O eekmac.$O eef3.$O eesrch.$O eequer.$O \
      eefill.$O eediag.$O sbstr.$O sbm.$O sberr.$O sbbcpy.$O

# It probably isn't necessary to make all this stuff all the time, but it
# is fairly easy and makes the whole process simpler.  If this is not done,
# the dependencies are very complicated because some of the .c and .h files
# are made dynamically.
elle:	ellec $(OBJ) $(FUN_OFILES) elle.h eesite.h
	@rm -rf elle
	@echo Start linking elle
#	$(CC) $(CFLAGS) defprf.c	# depends on the new *.h files
	$(CC) -i -o elle $(OBJ)
	@echo elle done.

defprf.c:	deffun.e
	cat deffun.e defprf.e | ellec -Pconf  > defprf.c

eefdef.h:	deffun.e
	cat deffun.e defprf.e | ellec -Fconf  > eefdef.h

eefidx.h:	deffun.e
	cat deffun.e defprf.e | ellec -FXconf > eefidx.h

# Don't flush these files if interrupted, dammit!
.PRECIOUS: ellec deffun.e defprf.e

# The following files must be recompiled if eefidx.h is changed
eecmds.$O eebuff.$O eeerr.$O eehelp.$O eejust.$O eemain.$O eeques.$O eef1.$O: eefidx.h

# ELLE profile compiler.  
#	Although eefdef.h and defprf.c are included by ELLEC, they
#	are not listed as dependencies in order to avoid loops (see
#	their target entries).  That is OK because their information is not
#	used when generating the makecf files; it only furnishes default
#	values needed when an ELLE user compiles a user profile.
ellec: ellec.c
	$(CC) -wa -o ellec ellec.c

clean:	
	@rm -f *.o *.s *.bak core elle ellec
