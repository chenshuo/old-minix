.define	_brk
.extern	__brk
.align 2
_brk: 
j __brk
