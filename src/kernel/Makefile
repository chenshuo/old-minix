# Makefile for kernel

# Directories
u = /usr
i = $u/include
s = $i/sys
h = $i/minix
l = $u/lib

# Programs, flags, etc.
CC =	exec cc
CPP =	$l/cpp
LD =	$(CC) -.o
CFLAGS = -m -I$i
LDFLAGS = -i

HEAD =	mpx.o

OBJS =	start.o protect.o klib.o table.o main.o proc.o \
	exception.o system.o clock.o memory.o tty.o keyboard.o \
	console.o i8259.o rs232.o rs2.o dmp.o misc.o driver.o \
	floppy.o wini.o at_wini.o bios_wini.o esdi_wini.o \
	ps_wini.o xt_wini.o printer.o aha_scsi.o dp8390.o \
	wdeth.o ne2000.o mcd.o sb16_dsp.o sb16_mixer.o

# What to make.
kernel: $(HEAD) $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(HEAD) $(OBJS)
	install -S 0 $@

all install:
	cd keymaps && $(MAKE) -$(MAKEFLAGS) $@

clean:
	cd keymaps && $(MAKE) -$(MAKEFLAGS) $@
	rm -f *.o *.bak kernel

# Dependencies
a =	kernel.h $h/config.h $h/const.h $h/type.h $h/syslib.h \
	$s/types.h $i/string.h $i/limits.h $i/errno.h \
	const.h type.h proto.h glo.h
d =	driver.h $h/callnr.h $h/com.h $h/partition.h proc.h

klib.o: $h/config.h $h/const.h const.h sconst.h protect.h
klib.o: klib88.s klib386.s
mpx.o: $h/config.h $h/const.h $h/com.h const.h protect.h sconst.h
mpx.o: mpx88.s mpx386.s
rs2.o: $h/config.h $h/const.h const.h protect.h sconst.h

clock.o:	$a
clock.o:	$i/signal.h
clock.o:	$h/callnr.h
clock.o:	$h/com.h
clock.o:	proc.h

console.o:	$a
console.o:	$i/sgtty.h
console.o:	$h/callnr.h
console.o:	$h/com.h
console.o:	protect.h
console.o:	tty.h

start.o:	$a
start.o:	$i/string.h
start.o:	$i/stdlib.h
start.o:	$h/boot.h
start.o:	protect.h

dmp.o:		$a
dmp.o:		$h/com.h
dmp.o:		proc.h
dmp.o:		tty.h

exception.o:	$a
exception.o:	$i/signal.h
exception.o:	proc.h

driver.o:	$a
driver.o:	$d
driver.o:	$s/ioctl.h

floppy.o:	$a
floppy.o:	$d
floppy.o:	$h/diskparm.h

i8259.o:	$a

keyboard.o:	$a
keyboard.o:	$i/sgtty.h
keyboard.o:	$i/signal.h
keyboard.o:	$i/unistd.h
keyboard.o:	$h/callnr.h
keyboard.o:	$h/com.h
keyboard.o:	$h/keymap.h
keyboard.o:	tty.h
keyboard.o:	keymaps/us-std.src

main.o:	$a
main.o:	$i/unistd.h
main.o:	$i/signal.h
main.o:	$h/callnr.h
main.o:	$h/com.h
main.o:	proc.h

memory.o:	$a
memory.o:	$d

misc.o:	$a
misc.o:	$i/stdlib.h
misc.o:	$h/com.h
misc.o:	assert.h

printer.o:	$a
printer.o:	$h/callnr.h
printer.o:	$h/com.h

proc.o:	$a
proc.o:	$h/callnr.h
proc.o:	$h/com.h
proc.o:	proc.h

protect.o:	$a
protect.o:	proc.h
protect.o:	protect.h

rs232.o:	$a
rs232.o:	$i/sgtty.h
rs232.o:	tty.h

system.o:	$a
system.o:	$i/signal.h
system.o:	$i/unistd.h
system.o:	$s/sigcontext.h
system.o:	$h/boot.h
system.o:	$h/callnr.h
system.o:	$h/com.h
system.o:	proc.h
system.o:	protect.h

table.o:	$a
table.o:	$h/com.h
table.o:	proc.h
table.o:	tty.h

tty.o:		$a
tty.o:		$i/sgtty.h
tty.o:		$s/ioctl.h
tty.o:		$i/signal.h
tty.o:		$i/fcntl.h
tty.o:		$h/callnr.h
tty.o:		$h/com.h
tty.o:		$h/keymap.h
tty.o:		proc.h
tty.o:		tty.h

aha_scsi.o:	$a
aha_scsi.o:	$d
aha_scsi.o:	$i/fcntl.h
aha_scsi.o:	$s/mtio.h
aha_scsi.o:	$s/ioctl.h

wini.o:	$a
wini.o:	$d

at_wini.o:	$a
at_wini.o:	$d
at_wini.o:	$s/ioctl.h

bios_wini.o:	$a
bios_wini.o:	$d

esdi_wini.o:	$a
esdi_wini.o:	$d

ps_wini.o:	$a
ps_wini.o:	$d

xt_wini.o:	$a
xt_wini.o:	$d

mcd.o:	$a
mcd.o:	$d
mcd.o:	$h/cdrom.h
mcd.o:	$s/ioctl.h

dp8390.o:	$a
dp8390.o:	$i/stdlib.h
dp8390.o:	$h/com.h
dp8390.o:	$i/net/hton.h
dp8390.o:	$i/net/gen/ether.h
dp8390.o:	$i/net/gen/eth_io.h
dp8390.o:	assert.h
dp8390.o:	protect.h
dp8390.o:	dp8390.h
dp8390.o:	proc.h

wdeth.o:	$a
wdeth.o:	$i/net/gen/ether.h
wdeth.o:	$i/net/gen/eth_io.h
wdeth.o:	assert.h
wdeth.o:	dp8390.h
wdeth.o:	wdeth.h

ne2000.o:	$a
ne2000.o:	$i/net/gen/ether.h
ne2000.o:	$i/net/gen/eth_io.h
ne2000.o:	dp8390.h
ne2000.o:	ne2000.h

sb16_dsp.o:	$a
sb16_dsp.o:	$h/com.h
sb16_dsp.o:	$h/callnr.h
sb16_dsp.o:	$s/ioctl.h
sb16_dsp.o:	sb16.h

sb16_mixer.o:	$a
sb16_mixer.o:	$h/com.h
sb16_mixer.o:	$h/callnr.h
sb16_mixer.o:	$s/ioctl.h
sb16_mixer.o:	$h/sound.h
sb16_mixer.o:	sb16.h
