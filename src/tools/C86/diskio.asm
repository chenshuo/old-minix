Title diskio  -  absolute disk read & write for MS-DOS
page,132
;
;	   int absread (disk,sector,&buffer)
;	   int disk, sector;
;	   char *buffer[PH_SECTSIZE];
;	and
;	   int abswrite (disk,sector,buffer)
;	   int disk, sector;
;	   char *buffer[PH_SECTSIZE];


INCLUDE \lib\C86\prologue.h
PUBLIC	absread, abswrite, dmaoverr


disk	equ	[bp+4]
sector	equ	[bp+6]
buffer	equ	[bp+8]


@CODE 	SEGMENT
	assume cs:@code,ds:dgroup

; These are the routines that do the DOS-interrupts; you can use
; them for any disk-drive in the system.
;
absread	PROC	NEAR
	push	bp
	mov	bp,sp

	mov	ax,disk
	mov	bx,buffer
	mov	cx,1
	mov	dx,sector

	int	25h		; abs disk-read

	jnc	rd_ok		; carry ===> error in al
	xor	ah,ah		; zero upper half
	inc	al		; code 0 means something too
	jmp	end_rd

rd_ok:	xor	ax,ax		; no error

end_rd: popf			; flags pushed by int
	pop	bp
	ret
absread	ENDP



abswrite PROC NEAR
	push	bp
	mov	bp,sp

	mov	ax,disk
	mov	bx,buffer
	mov	cx,1
	mov	dx,sector

	int	26h		; abs disk-write

	jnc	wr_ok
	xor	ah,ah
	inc	al
	jmp	end_wr

wr_ok:	xor	ax,ax

end_wr:	popf
	pop	bp
	ret

abswrite ENDP


dmaoverr PROC NEAR		; dummy routine
	 xor	ax,ax
         ret
dmaoverr ENDP

@CODE	ENDS

	END	; end of assembly file

