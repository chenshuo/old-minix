/* strspn.x
 *	size_t strspn(const char *s1, const char *s2)
 *
 *	Returns the length of the longest prefix of the string pointed
 *	to by s1 that is made up of the characters in the string s2.
 */

.define	_strspn
.text
_strspn:
	push	bp
	mov	bp,sp
	push	si
	push	di
	mov	si,4(bp)
	mov	di,6(bp)
	cld
	xor	bx,bx		/* default return value is zero */
	cmpb	(di),*0
	jz	exit		/* if s2 has length zero, we are done */
	cmpb	1(di),*0
	jz	find_mismatch	/* if s2 has length one, we take a shortcut */
	mov	cx,#-1		/* find length of s2 */
	xorb	al,al
	repne
	scab
	not	cx
	dec	cx
	mov	dx,cx		/* save length of s2 */
	dec	bx		/* set up byte count for faster loop */
s1_loop:			/* loop over s1 looking for matches with s2 */
	lodb
	inc	bx
	orb	al,al
	jz	exit
	mov	di,6(bp)
	mov	cx,dx
	repne
	scab
	je	s1_loop
	jmp	exit
find_mismatch:			/* find a character in s1 that is not *s2 */
	movb	al,(di)
	mov	di,si
	mov	cx,#-1
	repe
	scab
	dec	di		/* point back at mismatch */
	mov	bx,di
	sub	bx,si		/* number of matched characters */
exit:
	mov	ax,bx
	pop	di
	pop	si
	mov	sp,bp
	pop	bp
	ret
