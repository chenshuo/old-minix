.sect .text; .sect .rom; .sect .data; .sect .bss
.extern _modf
.sect .text
_modf:
	push	bp
	mov	bp,sp
lea bx,4(bp)
mov cx,#8
call .loi
mov dx,#1
push dx
push dx
push dx
mov ax,#2
push ax
call .cif8
mov ax,sp
push ax
call .fif8
pop bx
mov bx,12(bp)
mov cx,#8
call .sti
call .ret8
jmp .cret
