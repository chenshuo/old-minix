.define	_sigsuspend
.extern	__sigsuspend
.align 2
_sigsuspend: 
j __sigsuspend
