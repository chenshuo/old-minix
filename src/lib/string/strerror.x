/* strerror.x
 *	char *strerror(int errnum)
 *
 *	Returns a pointer to an appropriate error message string.
 */

.define	_strerror
.data
.extern	__sys_nerr, __sys_errlist
unknown: .asciz 'Unknown error'
.text
_strerror:
	mov	bx,sp
	mov	bx,2(bx)
	mov	ax,#unknown	/* default return is "Unknown error" */
	or	bx,bx
	jle	exit
	cmp	bx,__sys_nerr
	jge	exit
	sal	bx,#1
	mov	ax,__sys_errlist(bx)
exit:
	ret
