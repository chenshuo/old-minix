i=/usr/include
s=/usr/include/sys
h=/usr/include/minix
l=/usr/lib
a=mm.h $h/config.h $h/const.h $h/type.h $s/types.h $i/limits.h $i/errno.h \
  const.h type.h proto.h glo.h
CFLAGS = -F

obj =	main.s forkexit.s break.s exec.s signal.s  \
	alloc.s utility.s table.s putc.s trace.s getset.s

mm:	Makefile $l/head.s $(obj) $l/libc.a $l/end.s
	asld -i -s -o mm $l/head.s $(obj) $l/libc.a $l/end.s >symbol.out
	ast -X mm
	@rm symbol.out

clean:
	@rm -f $(obj) mm *.bak

alloc.s:	$a

break.s:	$a
break.s:	$i/signal.h
break.s:	mproc.h
break.s:	param.h

exec.s:		$a
exec.s:		$s/stat.h
exec.s:		$h/callnr.h
exec.s:		mproc.h
exec.s:		param.h

forkexit.s:	$a
forkexit.s:	$h/callnr.h
forkexit.s:	mproc.h
forkexit.s:	param.h

getset.s:	$a
getset.s:	$h/callnr.h
getset.s:	mproc.h
getset.s:	param.h

main.s:		$a
main.s:		$h/callnr.h
main.s:		$h/com.h
main.s:		mproc.h
main.s:		param.h

putc.s:		$a
putc.s:		$h/com.h

signal.s:	$a
signal.s:	$s/stat.h
signal.s:	$i/signal.h
signal.s:	$h/callnr.h
signal.s:	$h/com.h
signal.s:	mproc.h
signal.s:	param.h

table.s:	$a
table.s:	$h/callnr.h
table.s:	mproc.h
table.s:	param.h

trace.s:	$a
trace.s:	$i/signal.h
trace.s:	mproc.h
trace.s:	param.h

utility.s:	$a
utility.s:	$s/stat.h
utility.s:	$h/callnr.h
utility.s:	$h/com.h
utility.s:	mproc.h
