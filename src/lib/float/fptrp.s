#
.sect .text; .sect .rom; .sect .data; .sect .bss
.define __fptrp
.sect .text
__fptrp:
#if _EM_WSIZE == 2
	push	bp
	mov	bp, sp
	mov	ax, 4(bp)
	call	.Xtrp
	jmp	.cret
#else
	push	ebp
	mov	ebp, esp
	mov	eax, 8(bp)
	call	.Xtrp
	leave
	ret
#endif
