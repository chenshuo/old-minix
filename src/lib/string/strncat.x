/* strncat.x
 *	char *strncat(char *s1, const char *s2, size_t n)
 *
 *	Concatenates up to n characters of the string pointed to by s2
 *	onto the end of the string pointed to by s1.  A terminating
 *	null character is always appended.  Returns s1.
 */

.define	_strncat
.text
_strncat:
	mov	bx,si		/* save si and di */
	mov	dx,di
	mov	si,sp
	mov	cx,6(si)
	mov	di,2(si)
	push	di		/* save return value */
	jcxz	exit		/* early exit if n == 0 */
	cld
	mov	cx,#-1		/* find end of s1 */
	xorb	al,al
	repne
	scab
	dec	di
	mov	cx,6(si)
	mov	si,4(si)
byte_loop:			/* loop to copy bytes */
	lodb
	stob
	orb	al,al
	loopnz	byte_loop
	jz	exit
	movb	(di),*0		/* add terminating null character */
exit:
	mov	si,bx		/* restore si and di */
	mov	di,dx
	pop	ax
	ret
