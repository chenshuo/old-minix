# Makefile for commands/ibm.

# See commands/simple/Makefile for a description.

CFLAGS	= -D_MINIX -D_POSIX_SOURCE
CCLD	= $(CC) -i $(CFLAGS)
MAKE	= exec make -$(MAKEFLAGS)

all:	bin \
	bin/ar \
	bin/dosread \
	bin/fdisk \
	bin/format \
	bin/part \
	bin/partition \
	bin/postmort \
	bin/readclock \
	bin/repartition \
	bin/sdump \
	bin/term \

bin:
	mkdir bin

bin/ar:	ar.c
	$(CCLD) -o $@ $?
	install -S 4kw $@

bin/dosread:	dosread.c
	$(CCLD) -o $@ $?
	install -S 16kw $@

bin/fdisk:	fdisk.c
	$(CCLD) -o $@ $?
	install -S 4kw $@

bin/format:	format.c
	$(CCLD) -m -o $@ $?
	install -S 4kw $@

bin/part:	part.c
	$(CCLD) -m -o $@ $?
	install -S 10kw $@

bin/partition:	partition.c
	$(CCLD) -o $@ $?
	install -S 4kw $@

bin/postmort:	postmort.c
	$(CCLD) -m -o $@ $?
	install -S 4kw $@

bin/readclock:	readclock.c
	$(CCLD) -o $@ $?
	install -S 4kw $@

bin/repartition:	repartition.c
	$(CCLD) -o $@ $?
	install -S 4kw $@

bin/sdump:	sdump.c
	$(CCLD) -o $@ $?
	install -S 4kw $@

bin/term:	term.c
	$(CCLD) -o $@ -wa $?
	install -S 4kw $@

install:	bin \
	/usr/bin/ar \
	/usr/bin/dosread \
		/usr/bin/dosdir \
		/usr/bin/doswrite \
	/usr/bin/fdisk \
	/usr/bin/format \
	/usr/bin/part \
	/usr/bin/partition \
	/usr/bin/postmort \
	/usr/bin/readclock \
	/usr/bin/repartition \
	/usr/bin/sdump \
	/usr/bin/term \
	/bin/readclock \

/usr/bin/ar:	bin/ar
	install -cs -o bin $? $@

/usr/bin/dosread:	bin/dosread
	install -cs -o bin $? $@

/usr/bin/dosdir /usr/bin/doswrite:	/usr/bin/dosread
	install -l $? $@

/usr/bin/fdisk:	bin/fdisk
	install -cs -o bin $? $@

/usr/bin/format:	bin/format
	install -cs -o root -m 4755 $? $@

/usr/bin/part:	bin/part
	install -cs -o bin $? $@

/usr/bin/partition:	bin/partition
	install -cs -o bin $? $@

/usr/bin/postmort:	bin/postmort
	install -cs -o bin $? $@

/usr/bin/readclock:	bin/readclock
	install -cs -o bin $? $@

/usr/bin/repartition:	bin/repartition
	install -cs -o bin $? $@

/usr/bin/sdump:	bin/sdump
	install -cs -o bin $? $@

/usr/bin/term:	bin/term
	install -cs -o bin $? $@

/bin/readclock:	/usr/bin/readclock
	install -lcs $? $@

clean:
	rm -rf a.out core bin
