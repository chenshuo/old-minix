/* strerror.x
 *	char *strerror(int errnum)
 *
 *	Returns a pointer to an appropriate error message string.
 */

.define	_strerror
.data
.extern	_sys_nerr, _sys_errlist
unknown: .asciz 'Unknown error'
.text
_strerror:
	mov	bx,sp
	mov	bx,2(bx)
	mov	ax,#unknown	/* default return is "Unknown error" */
	or	bx,bx
	jle	exit
	cmp	bx,_sys_nerr
	jge	exit
	sal	bx,#1
	mov	ax,_sys_errlist(bx)
exit:
	ret
