# Makefile for aal

CC=exec cc
CFLAGS=-I. -wo -DAAL -DSTB -DNDEBUG -DDISTRIBUTION -D_POSIX_SOURCE -D_MINIX
LDFLAGS=-i -s

.SUFFIXES: .c .o
O=o
.c.$O:
	$(CC) -c $(CFLAGS) $< ;

OFILES=	archiver.$O \
	print.$O \
	rd.$O \
	rd_arhdr.$O \
	rd_unsig2.$O \
	sprint.$O \
	wr_arhdr.$O \
	wr_bytes.$O \
	wr_int2.$O \
	wr_long.$O \
	wr_ranlib.$O \
	format.$O \
	rd_bytes.$O \
	system.$O \
	write.$O \
	long2str.$O

aal: $(OFILES)
	$(CC) $(LDFLAGS) -o aal $(OFILES) ;

archiver.$O:
print.$O:
rd.$O:
rd_arhdr.$O:
rd_unsig2.$O:
sprint.$O:
wr_arhdr.$O:
wr_bytes.$O:
wr_int2.$O:
wr_long.$O:
wr_ranlib.$O:
format.$O:
rd_bytes.$O:
system.$O:
write.$O:
long2str.$O:

clean:	
	@rm -f *.s *.o core *.bak aal
