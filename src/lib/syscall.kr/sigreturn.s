.define	_sigreturn
.extern	__sigreturn
.align 2
_sigreturn: 
j __sigreturn
