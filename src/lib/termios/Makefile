# Makefile for lib/termios.

CFLAGS	= -O -D_MINIX -D_POSIX_SOURCE -m
CC1	= $(CC) $(CFLAGS) -c

LIBRARY	= ../libc.a
all:	$(LIBRARY)

OBJECTS	= \
	$(LIBRARY)(cfgetispeed.o) \
	$(LIBRARY)(cfgetospeed.o) \
	$(LIBRARY)(cfsetispeed.o) \
	$(LIBRARY)(cfsetospeed.o) \
	$(LIBRARY)(tcdrain.o) \
	$(LIBRARY)(tcflow.o) \
	$(LIBRARY)(tcflush.o) \
	$(LIBRARY)(tcsendbreak.o) \
	$(LIBRARY)(termios.o) \

$(LIBRARY):	$(OBJECTS)
	aal cr $@ *.o
	rm *.o

$(LIBRARY)(cfgetispeed.o):	cfgetispeed.c
	$(CC1) cfgetispeed.c

$(LIBRARY)(cfgetospeed.o):	cfgetospeed.c
	$(CC1) cfgetospeed.c

$(LIBRARY)(cfsetispeed.o):	cfsetispeed.c
	$(CC1) cfsetispeed.c

$(LIBRARY)(cfsetospeed.o):	cfsetospeed.c
	$(CC1) cfsetospeed.c

$(LIBRARY)(tcdrain.o):	tcdrain.c
	$(CC1) tcdrain.c

$(LIBRARY)(tcflow.o):	tcflow.c
	$(CC1) tcflow.c

$(LIBRARY)(tcflush.o):	tcflush.c
	$(CC1) tcflush.c

$(LIBRARY)(tcsendbreak.o):	tcsendbreak.c
	$(CC1) tcsendbreak.c

$(LIBRARY)(termios.o):	termios.c
	$(CC1) termios.c
