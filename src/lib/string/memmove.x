/* memmove.x
 *	void *memmove(void *s1, const void *s2, size_t n)
 *	void *memcpy(void *s1, const void *s2, size_t n)
 *
 *	Copy n characters from the object pointed to by s2 into the
 *	object pointed to by s1.  Copying takes place as if the n
 *	characters pointed to by s2 are first copied to a temporary
 *	area and then copied to the object pointed to by s1.
 *
 *	Per X3J11, memcpy may have undefined results if the objects
 *	overlap; since the performance penalty is insignificant, we
 *	use the safe memmove code for it as well.
 */

#define BYTE_LIMIT 10		/* if n is above this, work with words */

.define	_memmove, _memcpy
.text
_memmove:
_memcpy:
	mov	bx,si		/* save si and di */
	mov	dx,di
	mov	di,sp
	mov	cx,6(di)
	mov	si,4(di)
	mov	di,2(di)
	mov	ax,di		/* save a copy of s1 */
	jcxz	exit		/* early exit if n == 0 */
	sub	di,si
	je	exit		/* early exit if s1 == s2 */
	jb	left_to_right	/* left to right if s1 < s2 */
	cmp	di,cx
	jae	left_to_right	/* left to right if no overlap */
right_to_left:
	mov	di,ax		/* retrieve s1 */
	std
	add	si,cx		/* compute where objects end */
	dec	si
	add	di,cx
	dec	di
	cmp	cx,#BYTE_LIMIT
	jbe	byte_move
	test	si,#1		/* align source on word boundary */
	jnz	word_unaligned
	movb
	dec	cx
word_unaligned:
	dec	si		/* adjust to word boundary */
	dec	di
	shr	cx,#1		/* move words, not bytes */
	rep
	movw
	jnc	exit
#ifdef i8088
	inc	si		/* fix up addresses for right to left moves */
	inc	di
	movb			/* move leftover byte */
#else
	movb	cl,1(si)
	movb	1(di),cl	/* move leftover byte */
#endif
	jmp	exit
left_to_right:
	mov	di,ax		/* retrieve s1 */
	cld
	cmp	cx,#BYTE_LIMIT
	jbe	byte_move
	test	si,#1		/* align source on word boundary */
	jz	word_move
	movb
	dec	cx
word_move:
	shr	cx,#1		/* move words, not bytes */
	rep
	movw
	adc	cx,cx		/* set up to move leftover byte */
byte_move:
	rep
	movb
exit:
	cld			/* restore direction flag */
	mov	si,bx		/* restore si and di */
	mov	di,dx
	ret
