.sect .text; .sect .rom; .sect .data; .sect .bss
.extern __fptrp
.sect .text
__fptrp:
	push	bp
	mov	bp,sp
mov ax,4(bp)
call .Xtrp
jmp .cret
