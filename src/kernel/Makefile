# Makefile for kernel using standard Minix directory layout and compiler.
# CPP may need changing to /usr/lib/cpp.
# The following nonstandard flags are used:
# -F:	run cpp and cem sequentially (used when memory is tight)
# -T.:	put temporaries in working directory (when RAM disk is small)
BIN		=.
CC		=cc
CFLAGS		=-F -T.
CPP		=/usr/lib/cpp
CPPFLAGS	=-DASLD -P
END		=/usr/lib/end.s
LD		=asld
LDFLAGS		=-i
LIBS		=/usr/lib/libc.a
START		=start.s

a		=kernel.h $h/config.h $h/const.h $h/type.h \
		 $s/types.h $i/limits.h $i/errno.h \
		 const.h type.h proto.h glo.h
h		=/usr/include/minix
i		=/usr/include
s		=/usr/include/sys

KEROBJS		=cstart.s protect.s protect1.s klib.s mpx.s wini.s \
	table.s main.s tty.s floppy.s system.s proc.s \
	clock.s memory.s console.s rs232.s rs2.s printer.s \
	dmp.s exception.s i8259.s misc.s keyboard.s
SOBJS		=start.s klib.s mpx.s rs2.s

# Rules.
.SUFFIXES: .x		# .x files are .s files which need C-preprocessing
.x.s:
	$(CPP) $(CPPFLAGS) $< >$@

# What to make.
dummy: $(BIN)/kernel	# this line fixes a bug in RAL's latest make

$(BIN)/kernel: $(START) $(KEROBJS) $(LIBS) $(END)
	$(LD) $(LDFLAGS) -o $(BIN)/kernel \
	$(START) $(KEROBJS) \
	$(LIBS) $(END) -s > kernel.sym
	ast -X $(BIN)/kernel kernel.sym
	@rm kernel.sym

clean:
	@rm -f $(START) $(KEROBJS) $(SOBJS)

klib.s: $h/config.h $h/const.h const.h protect.h sconst.h klib.x
mpx.s: $h/config.h $h/const.h $h/com.h const.h protect.h sconst.h mpx.x
rs2.s: $h/config.h $h/const.h const.h sconst.h rs2.x
start.s: $h/config.h $h/const.h $h/com.h const.h protect.h sconst.h start.x

clock.s:	$a
clock.s:	$i/signal.h
clock.s:	$h/callnr.h
clock.s:	$h/com.h
clock.s:	proc.h

console.s:	$a
console.s:	$i/sgtty.h
console.s:	$h/callnr.h
console.s:	$h/com.h
console.s:	protect.h
console.s:	tty.h

cstart.s:	$a
cstart.s:	$h/boot.h
cstart.s:	cstart.c

dmp.s:		$a
dmp.s:		$h/callnr.h
dmp.s:		$h/com.h
dmp.s:		proc.h

exception.s:	$a
exception.s:	$i/signal.h
exception.s:	proc.h

floppy.s:	$a
floppy.s:	$h/callnr.h
floppy.s:	$h/com.h

i8259.s:	$a

keyboard.s:	$a
keyboard.s:	$i/sgtty.h
keyboard.s:	$i/signal.h
keyboard.s:	$h/callnr.h
keyboard.s:	$h/com.h
keyboard.s:	tty.h

main.s:	$a
main.s:	$i/signal.h
main.s:	$h/callnr.h
main.s:	$h/com.h
main.s:	proc.h

memory.s:	$a
memory.s:	$h/callnr.h
memory.s:	$h/com.h

misc.s:	$a
misc.s:	$h/com.h

printer.s:	$a
printer.s:	$h/callnr.h
printer.s:	$h/com.h

proc.s:	$a
proc.s:	$h/callnr.h
proc.s:	$h/com.h
proc.s:	proc.h

protect.s:	$a
protect.s:	protect.h
protect.s:	protect1.c

protect1.s:	$a
protect1.s:	proc.h
protect1.s:	protect.h

rs232.s:	$a
rs232.s:	$i/sgtty.h
rs232.s:	tty.h

system.s:	$a
system.s:	$i/signal.h
system.s:	$h/boot.h
system.s:	$h/callnr.h
system.s:	$h/com.h
system.s:	proc.h
system.s:	protect.h

table.s:	$a
table.s:	$h/com.h
table.s:	proc.h
table.s:	tty.h

tty.s:		$a
tty.s:		$i/sgtty.h
tty.s:		$i/signal.h
tty.s:		$h/callnr.h
tty.s:		$h/com.h
tty.s:		tty.h

wini.s:	$a
wini.s:	$h/callnr.h
wini.s:	$h/com.h
wini.s:	$h/partition.h


