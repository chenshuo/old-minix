CFLAGS= -F
i=/usr/include
s=/usr/include/sys
h=/usr/include/minix
l=/usr/lib
a=fs.h $h/config.h $h/const.h $h/type.h $s/types.h $i/limits.h $i/errno.h \
  const.h type.h proto.h glo.h

obj =	main.s open.s read.s write.s pipe.s device.s \
	path.s mount.s link.s super.s inode.s cache.s filedes.s \
	stadir.s protect.s time.s misc.s utility.s table.s \
	putc.s

fs:	Makefile $l/head.s $(obj) $l/libc.a $l/end.s
	asld -i -s -o fs $l/head.s $(obj) $l/libc.a $l/end.s >symbol.out
	ast -X fs
	@rm symbol.out

clean:
	@rm -f $(obj) fs *.bak

cache.s:	$a
cache.s:	$h/com.h
cache.s:	$h/boot.h
cache.s:	buf.h
cache.s:	file.h
cache.s:	fproc.h
cache.s:	inode.h
cache.s:	super.h

device.s:	$a
device.s:	$h/com.h
device.s:	dev.h
device.s:	file.h
device.s:	fproc.h
device.s:	inode.h
device.s:	param.h

filedes.s:	$a
filedes.s:	file.h
filedes.s:	fproc.h
filedes.s:	inode.h

inode.s:	$a
inode.s:	$s/stat.h
inode.s:	$h/boot.h
inode.s:	buf.h
inode.s:	file.h
inode.s:	fproc.h
inode.s:	inode.h
inode.s:	super.h

link.s:		$a
link.s:		$s/stat.h
link.s:		buf.h
link.s:		file.h
link.s:		fproc.h
link.s:		inode.h
link.s:		param.h

main.s:		$a
main.s:		$h/callnr.h
main.s:		$h/com.h
main.s:		$h/boot.h
main.s:		buf.h
main.s:		file.h
main.s:		fproc.h
main.s:		inode.h
main.s:		param.h
main.s:		super.h

misc.s:		$a
misc.s:		$i/fcntl.h
misc.s:		$h/callnr.h
misc.s:		$h/com.h
misc.s:		$h/boot.h
misc.s:		buf.h
misc.s:		file.h
misc.s:		fproc.h
misc.s:		inode.h
misc.s:		param.h
misc.s:		super.h

mount.s:	$a
mount.s:	$s/stat.h
mount.s:	buf.h
mount.s:	file.h
mount.s:	fproc.h
mount.s:	inode.h
mount.s:	param.h
mount.s:	super.h

open.s:		$a
open.s:		$i/fcntl.h
open.s:		$h/callnr.h
open.s:		buf.h
open.s:		file.h
open.s:		fproc.h
open.s:		inode.h
open.s:		param.h

path.s:		$a
path.s:		buf.h
path.s:		file.h
path.s:		fproc.h
path.s:		inode.h
path.s:		super.h

pipe.s:		$a
pipe.s:		$i/fcntl.h
pipe.s:		$i/signal.h
pipe.s:		$h/callnr.h
pipe.s:		$h/com.h
pipe.s:		dev.h
pipe.s:		file.h
pipe.s:		fproc.h
pipe.s:		inode.h
pipe.s:		param.h

protect.s:	$a
protect.s:	buf.h
protect.s:	file.h
protect.s:	fproc.h
protect.s:	inode.h
protect.s:	param.h
protect.s:	super.h

putc.s:		$a
putc.s:		$h/com.h

read.s:		$a
read.s:		$i/fcntl.h
read.s:		$h/com.h
read.s:		buf.h
read.s:		file.h
read.s:		fproc.h
read.s:		inode.h
read.s:		param.h
read.s:		super.h

stadir.s:	$a
stadir.s:	$s/stat.h
stadir.s:	file.h
stadir.s:	fproc.h
stadir.s:	inode.h
stadir.s:	param.h

super.s:	$a
super.s:	$h/boot.h
super.s:	buf.h
super.s:	inode.h
super.s:	super.h

table.s:	$a
table.s:	$s/stat.h
table.s:	$h/callnr.h
table.s:	$h/com.h
table.s:	buf.h
table.s:	dev.h
table.s:	file.h
table.s:	fproc.h
table.s:	inode.h
table.s:	super.h

time.s:		$a
time.s:		$h/callnr.h
time.s:		$h/com.h
time.s:		file.h
time.s:		fproc.h
time.s:		inode.h
time.s:		param.h

utility.s:	$a
utility.s:	$h/com.h
utility.s:	$h/boot.h
utility.s:	buf.h
utility.s:	file.h
utility.s:	fproc.h
utility.s:	inode.h
utility.s:	param.h
utility.s:	super.h

write.s:	$a
write.s:	buf.h
write.s:	file.h
write.s:	fproc.h
write.s:	inode.h
write.s:	super.h
