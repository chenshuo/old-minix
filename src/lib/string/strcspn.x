/* strcspn.x
 *	size_t strcspn(const char *s1, const char *s2)
 *
 *	Returns the length of the longest prefix of the string pointed
 *	to by s1 that has none of the characters in the string s2.
 */

.define	_strcspn
.text
_strcspn:
	push	bp
	mov	bp,sp
	push	si
	push	di
	mov	si,4(bp)
	mov	di,6(bp)
	cld
	mov	bx,#-1		/* set up count (-1 for faster loops) */
	cmpb	(di),*0
	jz	s1_length	/* if s2 is null, we return length of s1 */
	cmpb	1(di),*0
	jz	find_match	/* if s2 has length one, we take a shortcut */
	mov	cx,bx		/* find length of s2 */
	xorb	al,al
	repne
	scasb
	not	cx
	dec	cx
	mov	dx,cx		/* save length of s2 */
s1_loop:			/* loop over s1 looking for matches with s2 */
	lodsb
	inc	bx
	orb	al,al
	jz	exit
	mov	di,6(bp)
	mov	cx,dx
	repne
	scasb
	jne	s1_loop
	jmp	exit
s1_length:			/* find length of s1 */
	mov	di,si
	mov	cx,bx
	xorb	al,al
	repne
	scasb
	not	cx
	dec	cx
	mov	bx,cx
	jmp	exit
find_match:			/* find a match for *s2 in s1 */
	movb	dl,(di)
	test	si,#1		/* align source on word boundary */
	jz	word_loop
	lodsb
	inc	bx
	orb	al,al
	je	exit
	cmpb	al,dl
	je	exit
word_loop:
	lods
	inc	bx
	orb	al,al
	je	exit
	cmpb	al,dl
	je	exit
	inc	bx
	orb	ah,ah
	je	exit
	cmpb	ah,dl
	jne	word_loop
exit:
	mov	ax,bx
	pop	di
	pop	si
	mov	sp,bp
	pop	bp
	ret
