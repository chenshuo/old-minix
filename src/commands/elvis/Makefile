# Makefile for elvis

# Several groups of Makefile settings are included below.  Choose *ONE* group
# of settings for your particular system, and leave the others commented out.
# The meanings of these settings are:
#	O	the filename extension for unlinked object files -- usually .o
#	EXTRA	version-specific object files used in elvis
#	CC	the C compiler command, possibly with "memory model" flags
#	CFLAGS	compiler flags used to select compile-time options
#	PROGS	the list of all programs
#	SORT	if the "tags" file must be sorted, then SORT=-DSORT

O=s
PROGS=	elvis ctags ref elvrec fmt elvprsv

#---- These settings are recommended for Minix-PC ----
EXTRA=	tinytcap.$O tinyprnt.$O
CFLAGS=	-O -w -D_POSIX_SOURCE -D_MINIX -DCRUNCH \
	-DNO_MKEXRC -DNO_CURSORSHAPE -DNO_CHARATTR -DNO_SHOWMODE \
	-DNO_MODELINE -DNO_OPTCOLS -DNO_DIGRAPH -DNO_EXTENSIONS \
	-DNO_ERRLIST -DNO_FKEY -DNO_VISIBLE -DNO_COLOR -DNO_POPUP

#---- These settings are recommended for Minix-ST ----
#EXTRA=
#CFLAGS=

###########################################################################
###     The rest of this Makefile contains no user-serviceable parts    ###
###########################################################################

OBJ=blk.$O cmd1.$O cmd2.$O ctype.$O curses.$O cut.$O ex.$O input.$O main.$O misc.$O \
   modify.$O move1.$O move2.$O move3.$O move4.$O move5.$O opts.$O recycle.$O \
   redraw.$O regexp.$O regsub.$O system.$O tio.$O tmp.$O unix.$O vars.$O vcmd.$O vi.$O

all:	$(PROGS)
	@echo all done.

elvis:	$(OBJ)
	@rm -rf elvis
	@echo Start linking elvis
	$(CC) -i -o elvis $(OBJ) >/dev/null
	@chmem =35000 elvis

ctags:	ctags.c
	$(CC) $(CFLAGS) $(SORT) -o ctags ctags.c

ref:	ref.c
	$(CC) $(CFLAGS) -o ref ref.c

elvrec:	elvrec.c
	$(CC) $(CFLAGS) -o elvrec elvrec.c

fmt:	fmt.c
	$(CC) $(CFLAGS) -o fmt fmt.c

elvprsv:	elvprsv.c
	$(CC) $(CFLAGS) -o elvprsv elvprsv.c ctype.$O

# Dependencies
$(OBJ):	vi.h curses.h config.h regexp.h ctype.h

clean:	
	@rm -f *.o *.s ctags ref elvrec fmt elvprsv elvis
