# The kernel dir contains xt_wini.c and at_wini.c.  Before running make
# you must copy one of these to wini.c, depending on which controller you
# have.  If you do not have a hard disk, you MUST choose one of them at random.
# On a PC, cpp and cem are in /lib and will be removed to make space while
# linking the kernel.  On an AT, they are in /usr/lib are are not removed.
# This is because they have to be in /lib on a PC; the diskette is too small
# for them to be in /usr/lib.  You can change this by editing commands/cc.c.
#
# The CFLAGS values are:
#  -Di8088	- required on an 8088/80286/80386 CPU; forbidden on a 68000
#  -F		- run cpp and cem sequentially (used when memory is tight)
#  -T.		- put temporaries in working directory (when RAM disk is small)
#
CFLAGS= -Di8088 -F -T.
h=../h
l=/usr/lib

obj =	mpx88.s main.s tty.s floppy.s wini.s system.s proc.s clock.s memory.s \
	console.s rs232.s printer.s table.s klib88.s dmp.s

cobjs =	main.s tty.s floppy.s wini.s system.s proc.s clock.s memory.s \
	console.s rs232.s printer.s table.s dmp.s

kernel:	makefile $(obj) 
	@echo "Start linking Kernel."
	@asld -o kernel $(obj) $l/libc.a $l/end.s
	@echo "Kernel done.  "

clean:
	rm -f $(cobjs)

clock.s:	const.h type.h $h/const.h $h/type.h
clock.s:	$h/callnr.h
clock.s:	$h/com.h
clock.s:	$h/error.h
clock.s:	$h/signal.h
clock.s:	glo.h
clock.s:	proc.h

console.s:	const.h type.h $h/const.h $h/type.h
console.s:	$h/callnr.h
console.s:	$h/com.h
console.s:	$h/error.h
console.s:	$h/sgtty.h
console.s:	$h/signal.h
console.s:	glo.h
console.s:	proc.h
console.s:	tty.h
console.s:	ttymaps.h

floppy.s:	const.h type.h $h/const.h $h/type.h
floppy.s:	$h/callnr.h
floppy.s:	$h/com.h
floppy.s:	$h/error.h
floppy.s:	glo.h
floppy.s:	proc.h


dmp.s:		const.h type.h $h/const.h $h/type.h
dmp.s:		$h/callnr.h
dmp.s:		$h/com.h
dmp.s:		$h/error.h
dmp.s:		glo.h
dmp.s:		proc.h

main.s:		const.h type.h $h/const.h $h/type.h
main.s:		$h/callnr.h
main.s:		$h/com.h
main.s:		$h/error.h
main.s:		glo.h
main.s:		proc.h

memory.s:	const.h type.h $h/const.h $h/type.h
memory.s:	$h/callnr.h
memory.s:	$h/com.h
memory.s:	$h/error.h
memory.s:	proc.h

printer.s:	const.h type.h $h/const.h $h/type.h
printer.s:	$h/callnr.h
printer.s:	$h/com.h
printer.s:	$h/error.h
printer.s:	proc.h
printer.s:	glo.h

proc.s:		const.h type.h $h/const.h $h/type.h
proc.s:		$h/callnr.h
proc.s:		$h/com.h
proc.s:		$h/error.h
proc.s:		glo.h
proc.s:		proc.h

rs232.s:	const.h type.h $h/const.h $h/type.h
rs232.s:	$h/callnr.h
rs232.s:	$h/com.h
rs232.s:	$h/error.h
rs232.s:	$h/sgtty.h
rs232.s:	$h/signal.h
rs232.s:	glo.h
rs232.s:	proc.h
rs232.s:	tty.h

system.s:	const.h type.h $h/const.h $h/type.h
system.s:	$h/callnr.h
system.s:	$h/com.h
system.s:	$h/error.h
system.s:	$h/signal.h
system.s:	glo.h
system.s:	proc.h

table.s:	const.h type.h $h/const.h $h/type.h $h/com.h
table.s:	glo.h
table.s:	proc.h
table.s:	tty.h

tty.s:	const.h type.h $h/const.h $h/type.h
tty.s:	$h/callnr.h
tty.s:	$h/com.h
tty.s:	$h/error.h
tty.s:	$h/sgtty.h
tty.s:	$h/signal.h
tty.s:	glo.h
tty.s:	proc.h
tty.s:	tty.h
tty.s:	ttymaps.h
tty.s:	tty.h
tty.s:	ttymaps.h

wini.s:	const.h type.h $h/const.h $h/type.h
wini.s:	$h/callnr.h
wini.s:	$h/com.h
wini.s:	$h/error.h
wini.s:	proc.h
