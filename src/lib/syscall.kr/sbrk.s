.define	_sbrk
.extern	__sbrk
.align 2
_sbrk: 
j __sbrk
