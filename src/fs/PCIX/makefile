CFLAGS = -Di8088  -O
h=../h
l=../lib

obj =	main.o open.o read.o write.o pipe.o device.o \
	path.o mount.o link.o super.o inode.o cache.o filedes.o \
	stadir.o protect.o time.o misc.o utility.o table.o putc.o

fs:	makefile  $l/head.o $(obj) $l/libc.a $l/end.o
	@echo "Start linking FS"
	@ld -o fs $l/head.o $(obj) $l/libc.a $l/end.o
	@echo "FS done"

cache.o:	const.h type.h $h/const.h $h/type.h
cache.o:	$h/error.h
cache.o:	buf.h
cache.o:	file.h
cache.o:	fproc.h
cache.o:	glo.h
cache.o:	inode.h
cache.o:	super.h

device.o:	const.h type.h $h/const.h $h/type.h
device.o:	$h/com.h
device.o:	$h/error.h
device.o:	dev.h
device.o:	file.h
device.o:	fproc.h
device.o:	glo.h
device.o:	inode.h
device.o:	param.h

filedes.o:	const.h type.h $h/const.h $h/type.h
filedes.o:	$h/error.h
filedes.o:	file.h
filedes.o:	fproc.h
filedes.o:	glo.h
filedes.o:	inode.h

inode.o:	const.h type.h $h/const.h $h/type.h
inode.o:	$h/error.h
inode.o:	buf.h
inode.o:	file.h
inode.o:	fproc.h
inode.o:	glo.h
inode.o:	inode.h
inode.o:	super.h

link.o:		const.h type.h $h/const.h $h/type.h
link.o:		$h/error.h
link.o:		buf.h
link.o:		file.h
link.o:		fproc.h
link.o:		glo.h
link.o:		inode.h
link.o:		param.h

main.o:		const.h type.h $h/const.h $h/type.h
main.o:		$h/callnr.h
main.o:		$h/com.h
main.o:		$h/error.h
main.o:		buf.h
main.o:		file.h
main.o:		fproc.h
main.o:		glo.h
main.o:		inode.h
main.o:		param.h
main.o:		super.h

misc.o:		const.h type.h $h/const.h $h/type.h
misc.o:		$h/callnr.h
misc.o:		$h/com.h
misc.o:		$h/error.h
misc.o:		buf.h
misc.o:		file.h
misc.o:		fproc.h
misc.o:		glo.h
misc.o:		inode.h
misc.o:		param.h
misc.o:		super.h

mount.o:	const.h type.h $h/const.h $h/type.h
mount.o:	$h/error.h
mount.o:	buf.h
mount.o:	file.h
mount.o:	fproc.h
mount.o:	glo.h
mount.o:	inode.h
mount.o:	param.h
mount.o:	super.h

open.o:		const.h type.h $h/const.h $h/type.h
open.o:		$h/callnr.h
open.o:		$h/error.h
open.o:		buf.h
open.o:		file.h
open.o:		fproc.h
open.o:		glo.h
open.o:		inode.h
open.o:		param.h

path.o:		const.h type.h $h/const.h $h/type.h
path.o:		$h/error.h
path.o:		buf.h
path.o:		file.h
path.o:		fproc.h
path.o:		glo.h
path.o:		inode.h
path.o:		super.h

pipe.o:		const.h type.h $h/const.h $h/type.h
pipe.o:		$h/callnr.h
pipe.o:		$h/com.h
pipe.o:		$h/error.h
pipe.o:		$h/signal.h
pipe.o:		file.h
pipe.o:		fproc.h
pipe.o:		glo.h
pipe.o:		inode.h
pipe.o:		param.h

protect.o:	const.h type.h $h/const.h $h/type.h
protect.o:	$h/error.h
protect.o:	buf.h
protect.o:	file.h
protect.o:	fproc.h
protect.o:	glo.h
protect.o:	inode.h
protect.o:	param.h
protect.o:	super.h

putc.o:		const.h type.h $h/const.h $h/type.h
putc.o:		$h/com.h

read.o:		const.h type.h $h/const.h $h/type.h
read.o:		$h/com.h
read.o:		$h/error.h
read.o:		buf.h
read.o:		file.h
read.o:		fproc.h
read.o:		glo.h
read.o:		inode.h
read.o:		param.h
read.o:		super.h

stadir.o:	const.h type.h $h/const.h $h/type.h
stadir.o:	$h/error.h
stadir.o:	$h/stat.h
stadir.o:	file.h
stadir.o:	fproc.h
stadir.o:	glo.h
stadir.o:	inode.h
stadir.o:	param.h

super.o:	const.h type.h $h/const.h $h/type.h
super.o:	$h/error.h
super.o:	buf.h
super.o:	inode.h
super.o:	super.h

table.o:	const.h type.h $h/const.h $h/type.h
table.o:	$h/com.h
table.o:	$h/callnr.h
table.o:	$h/error.h
table.o:	$h/stat.h
table.o:	buf.h
table.o:	dev.h
table.o:	file.h
table.o:	fproc.h
table.o:	glo.h
table.o:	inode.h
table.o:	super.h

time.o:		const.h type.h $h/const.h $h/type.h
time.o:		$h/callnr.h
time.o:		$h/com.h
time.o:		$h/error.h
time.o:		file.h
time.o:		fproc.h
time.o:		glo.h
time.o:		inode.h
time.o:		param.h

utility.o:	const.h type.h $h/const.h $h/type.h
utility.o:	$h/com.h
utility.o:	$h/error.h
utility.o:	buf.h
utility.o:	file.h
utility.o:	fproc.h
utility.o:	glo.h
utility.o:	inode.h
utility.o:	param.h
utility.o:	super.h

write.o:	const.h type.h $h/const.h $h/type.h
write.o:	$h/error.h
write.o:	buf.h
write.o:	file.h
write.o:	fproc.h
write.o:	glo.h
write.o:	inode.h
write.o:	super.h
