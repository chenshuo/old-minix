/* strpbrk.x
 *	char *strpbrk(const char *s1, const char *s2)
 *
 *	Returns the address of the first character of the string pointed
 *	to by s1 that is in the string pointed to by s2.  Returns NULL
 *	if no such character exists.
 */

.define	_strpbrk
.text
_strpbrk:
	mov	bx,sp
	push	si
	push	di
	mov	si,2(bx)
	mov	di,4(bx)
	mov	bx,di		/* save a copy of s2 */
	cld
	xor	ax,ax		/* default return value is NULL */
	cmpb	(di),*0
	jz	exit		/* if s2 has length zero, we are done */
	cmpb	1(di),*0
	jz	find_match	/* if s2 has length one, we take a shortcut */
	mov	cx,#-1		/* find length of s2 */
	repne
	scasb
	not	cx
	dec	cx
	mov	dx,cx		/* save length of s2 */
s1_loop:			/* loop through s1 to find matches with s2 */
	lodsb
	orb	al,al
	jz	exit
	mov	di,bx
	mov	cx,dx
	repne
	scasb
	jne	s1_loop
#ifdef i8088
	dec	si
	mov	ax,si
#else
	lea	ax,-1(si)
#endif
	pop	di
	pop	si
	ret
find_match:			/* find a match for *s2 in s1 */
	movb	dl,(di)
	test	si,#1		/* align source on word boundary */
	jz	word_loop
	lodsb
	cmpb	al,dl
	je	one_past
	orb	al,al
	jz	no_match
word_loop:
	lods
	cmpb	al,dl
	je	two_past
	orb	al,al
	jz	no_match
	cmpb	ah,dl
	je	one_past
	orb	ah,ah
	jnz	word_loop
no_match:
	xor	ax,ax
	pop	di
	pop	si
	ret
two_past:
	dec	si
one_past:
#ifdef i8088
	dec	si
	mov	ax,si
#else
	lea	ax,-1(si)
#endif
exit:
	pop	di
	pop	si
	ret
