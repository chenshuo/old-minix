.define	_isatty
.extern	__isatty
.align 2
_isatty: 
j __isatty
