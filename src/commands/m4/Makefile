# Makefile for M4

#	-DEXTENDED	#if you like to get paste & spaste macros.
#	-DVOID 		#if your C compiler does NOT support void.
#	-DGETOPT	#if you STILL do not have getopt in your library.
#	-DDUFFCP	#if you do not have fast memcpy in your library.
#

CFLAGS = -DEXTENDED -O -D_POSIX_SOURCE -D_MINIX
O=o

OBJ =  main.$O eval.$O serv.$O look.$O misc.$O expr.$O
INCL =  mdef.h extr.h patchlevel.h

m4: $(OBJ) $(INCL)
	@rm -rf m4
	@echo Start linking m4
	@cc -o m4 $(OBJ) >/dev/null
	@chmem =8192 m4


clean:	
	@rm -f *.o *.s m4 core *bak
