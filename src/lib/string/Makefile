.SUFFIXES: .o .x .s
CFLAGS= -Di8088 -D_POSIX_SOURCE -D_MINIX
.x.o:
	/usr/lib/cpp $(CFLAGS) $< | as -o $*.o

OBJECTS = memchr.o memcmp.o memcpy.o memmove.o memset.o \
	strcat.o strchr.o strcmp.o strcoll.o strcpy.o strcspn.o \
	strerror.o strlen.o strncat.o strncmp.o strncpy.o strpbrk.o \
	strrchr.o strspn.o strstr.o strtok.o strxfrm.o

all: $(OBJECTS)

clean:	
	@rm -f *.o *.s *.bak
