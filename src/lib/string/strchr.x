/* strchr.x
 *	char *strchr(const char *s, int c)
 *
 *	Returns location of the first occurrence of c (converted to char)
 *	in the string pointed to by s.  Returns NULL if c does not occur.
 */

.define	_strchr
.text
_strchr:
	mov	bx,si		/* save si */
	mov	si,sp
	movb	dl,4(si)
	mov	si,2(si)
	cld
	test	si,#1		/* align string on word boundary */
	jz	word_loop
	lodsb
	cmpb	al,dl
	je	one_past
	orb	al,al
	jz	no_match
word_loop:			/* look for c word by word */
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
	mov	si,bx		/* restore si */
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
	mov	si,bx		/* restore si */
	ret
