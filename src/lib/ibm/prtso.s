| Pascal Run Time Start Off routine (analogous to crtso.s for C programs)
| This routine is used only with the MINIX Pascal compiler.

.globl _m_a_i_n, _exit, prtso, _environ
.globl begtext, begdata, begbss
.globl hol0
.text
begtext:
prtso:
mov bx,sp
mov cx,(bx)
add bx,*2
mov ax,cx
inc ax
shl ax,#1
add ax,bx
mov _environ,ax 
push ax 
push bx 
push cx 
call _m_a_i_n
add sp,*6
push ax 
call _exit

.data
begdata:
_environ: .word 0
hol0:
.word  0,0
.word  0,0

.bss
begbss:
