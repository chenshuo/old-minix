# Copyright 1989 by Norbert Schlenker.  All rights reserved.
# The copyright holder hereby grants to the public the right to use,
# modify, and redistribute this software freely.
# This software is provided "as is" and carries no warranty of any kind.


# Should reflect the location of your C preprocessor.
CPP = /usr/lib/cpp

# Should reflect the location of your headers.
INCLUDE = /usr/include

# Should reflect the location of your library.
LIBC = /usr/lib/libc.a

# Should reflect the target machine.
TARGET = i8088
#TARGET = i80286
#TARGET = i80386

# The rest should be fine as is.
DIFFS = string.h.cdif

SRCS =	memchr.x memcmp.x memmove.x memset.x strcat.x strchr.x \
	strcmp.x strcoll.x strcpy.x strcspn.x strerror.x strlen.x \
	strncat.x strncmp.x strncpy.x strpbrk.x strrchr.x strspn.x \
	strstr.x strtok.x strxfrm.x

OBJS =	memchr.s memcmp.s memmove.s memset.s strcat.s strchr.s \
	strcmp.s strcoll.s strcpy.s strcspn.s strerror.s strlen.s \
	strncat.s strncmp.s strncpy.s strpbrk.s strrchr.s strspn.s \
	strstr.s strtok.s strxfrm.s

.SUFFIXES: .s .x .o .c .y

.x.s:
	$(CPP) -P -D$(TARGET) $*.x | libpack >$*.s

objects: $(OBJS)

perf:	operf nperf analyze
	@echo
	@echo Analyzing relative performance of old and new string packages ...
	@echo '   This test may take up to 10 minutes to run.'
	@echo
	@operf >operf.out
	@nperf >nperf.out
	@analyze operf.out nperf.out | tee Perf.local

operf:	perf.s
	cc -o operf perf.s

nperf:	perf.s $(OBJS)
	cc -o nperf perf.s $(OBJS)

analyze: analyze.c
	cc -o analyze analyze.c

install: $(OBJS)
	patch /usr/include/string.h string.h.cdif
	ar dv $(LIBC) memcpy.s		# Spencer's memcpy replaced by memmove
	ar rlv $(LIBC) $?

string.shar: Readme Copyright Makefile Perf.T5100 perf.c analyze.c $(DIFFS) $(SRCS)
	shar \
	  Readme Copyright Makefile Perf.T5100 \
	  perf.c analyze.c \
	  $(DIFFS) \
	  $(SRCS) \
	>string.shar

clean:
	@rm -f operf nperf analyze operf.out nperf.out Perf.local *.s
