.define __send, __receive, __sendrec
SEND = 1
RECEIVE = 2
BOTH = 3
SYSVEC = 32
.extern __send, __receive, __sendrec
__send: mov cx,*SEND
j L0
__receive: 
mov cx,*RECEIVE
j L0
__sendrec: 
mov cx,*BOTH
j L0
L0: push bp
mov bp,sp
mov ax,4(bp)
mov bx,6(bp)
int SYSVEC
pop bp
ret
