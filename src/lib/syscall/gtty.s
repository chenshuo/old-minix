.sect .text
.extern	__gtty
.define	_gtty
.align 2

_gtty:
	jmp	__gtty
