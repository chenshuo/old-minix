/* memchr.x
 *	void *memchr(const void *s, int c, size_t n)
 *
 *	Returns a pointer to the first occurrence of c (converted to
 *	unsigned char) in the object pointed to by s, NULL if none.
 */

.define	_memchr
.text
_memchr:
	mov	bx,di		/* save di */
	mov	di,sp
	xor	dx,dx		/* default result is NULL */
	mov	cx,6(di)
	jcxz	exit		/* early exit if n == 0 */
	movb	al,4(di)
	mov	di,2(di)
	cld
	repne
	scab
	jne	exit
#ifdef i8088
	dec	di
	mov	dx,di
#else
	lea	dx,-1(di)
#endif
exit:
	mov	di,bx		/* restore di */
	mov	ax,dx
	ret
