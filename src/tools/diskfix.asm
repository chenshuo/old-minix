
	title	'Diskfix'

;	Author:	Adri Koppes
;		(c) 1986
;
;
;	This program changes all Dos partitions on a winchester
;	to Minix partitions.
;	It must be called with the drive to change
;	(either 0 or 1).

SECTOR_SIZE	equ	512		;size of one disk-sector
STACK_SIZE	equ	256		;size of stack
PR_STRING	equ	9		;function-code to print a string
DISK_IO		equ	13H		;interrupt-number to do diskio
DOS		equ	21H		;interrupt-number for doscalls
DOS_TYPE	equ	1		;type of dos-partition
MINIX_TYPE	equ	40H		;type of minix-partition
PART_OFFSET	equ	01C2H		;offset of partition-table
NR_OF_PART	equ	4		;number of partitions
PART_LEN	equ	10H		;length of partition in bytes

dseg	segment para public 'data'
buf	db	SECTOR_SIZE dup (?)	;sector buffer
drive	dw	?			;drive-number to use
us_mes	db	'Usage: diskfix <drive> (<drive> = 0 or 1)',0DH,0AH,'$'
rd_mes	db	'Error reading partition-table',0DH,0AH,'$'
wr_mes	db	'Error writing partition-table',0DH,0AH,'$'
dseg	ends

sseg	segment stack 'stack'
	dw	STACK_SIZE/2 dup (?)	;stack space
sseg	ends

cseg	segment	para public 'code'
	assume	cs:cseg,ds:dseg,es:nothing,ss:sseg

main	proc	far			;main program
	cli				;no interrupts now
	mov	ax,sseg			;setup stack
	mov	ss,ax
	mov	sp,STACK_SIZE
	sti				;interrupts back on
	push	ds			;setup return to dos
	mov	ax,0
	push	ax
	mov	ax,ds			;setup data segments
	mov	es,ax
	mov	ax,dseg
	mov	ds,ax
	call	parse			;parse command line
	push	ds
	pop	es
	call	rd_part			;read partition-table from disk
	jc	rd_err			;check if error occurred
	call	change			;change partition types
	call	wr_part			;write partition-table to disk
	jc	wr_err			;check if error occurred
	jmp	done			;exit program
usage:	mov	dx,offset us_mes	;get usage string
	call	print			;and print it
	add	sp,2			;adjust stack pointer
	jmp	done			;and exit program
rd_err:	mov	dx,offset rd_mes	;get error string
	call	print			;and print it
	jmp	done			;exit program
wr_err:	mov	dx,offset wr_mes	;get error string
	call	print			;and print it
done:	ret				;exit program
main	endp				;end of main program

print	proc	near			;procedure to print a string
	mov	ah,PR_STRING		;pointer to the string is in dx
	int	DOS
	ret
print	endp

rd_part	proc	near			;procedure to read the partition-table
	mov	ax,0201h		;read one sector
	mov	bx,offset buf		;into buf
	mov	cx,1			;starting at first sector
	mov	dx,drive		;of `drive'
	int	DISK_IO			;perform diskio
	ret
rd_part	endp

wr_part	proc	near			;procedure to write the partition-table
	mov	ax,0301h		;write one sector
	mov	bx,offset buf		;from buf
	mov	cx,1			;starting at first sector
	mov	dx,drive		;of `drive'
	int	DISK_IO			;perform diskio
	ret
wr_part	endp

change	proc	near			;procedure to change partition types
	mov	di,offset buf+PART_OFFSET ;di = pointer to first partition-type
	mov	cx,NR_OF_PART		;cx = number of partitions
next:	mov	al,[di]			;get partition-type
	cmp	al,DOS_TYPE		;is it DOS?
	jne	again			;no, do next
	mov	byte ptr [di],MINIX_TYPE ;yes, change type to MINIX
again:	add	di,PART_LEN		;increment pointer
	loop	next			;do next
	ret
change	endp

parse	proc	near			;procedure to parse command line
	mov	al,es:82H		;get one character
	cmp	al,'0'			;is it a 0?
	jne	parse1			;no, continue
	mov	drive,80H		;set drive to winchester 0
	jmp	parse2			;exit procedure
parse1:	cmp	al,'1'			;is it a 1?
	jne	usage			;print a error message
	mov	drive,81H		;set drive to winchester 1
parse2:	ret
parse	endp
cseg	ends
	end	main
