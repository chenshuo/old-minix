.sect .text; .sect .rom; .sect .data; .sect .bss
.extern _frexp
.sect .text
_frexp:
	push	bp
	mov	bp,sp
lea bx,4(bp)
mov cx,#8
call .loi
mov ax,sp
add ax,#-2
push ax
call .fef8
mov bx,12(bp)
pop (bx)
! kill bx
call .ret8
jmp .cret
