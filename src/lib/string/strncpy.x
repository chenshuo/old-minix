/* strncpy.x
 *	char *strncpy(char *s1, const char *s2, size_t n)
 *
 *	Copy up to n characters from the string pointed to by s2 to
 *	the array pointed to by s1.  If the source string is shorter
 *	than n characters, the remainder of the destination is padded
 *	with null characters.  If the source is longer than n characters,
 *	the destination will not be null terminated.  Returns s1.
 */

#define BYTE_LIMIT 10		/* if n is above this, zero fill with words */

.define	_strncpy
.text
_strncpy:
	mov	bx,sp
	push	si
	push	di
	mov	cx,6(bx)
	jcxz	exit		/* early exit if n == 0 */
	mov	di,2(bx)
	mov	si,4(bx)
	cld
	cmpb	(si),*0
	je	zero_fill	/* if s2 has length zero, take a short cut */
	test	si,#1		/* align source on word boundary */
	jz	set_length
	movb
	dec	cx
	jz	exit		/* early exit if n == 1 */
set_length:
	mov	dx,cx		/* save count */
	shr	cx,#1		/* copy words, not bytes */
	jz	last_byte
word_copy:			/* loop to copy words */
	lods
	orb	al,al
	jz	restore_length	/* early exit if low byte == 0 */
	stos
	orb	ah,ah
	loopnz	word_copy
	jz	restore_length
last_byte:
	test	dx,#1		/* move leftover byte */
	jz	exit
	movb
	jmp	exit
restore_length:			/* retrieve remaining length (in bytes) */
	shl	cx,#1
	and	dx,#1
	add	cx,dx
	jz	exit
zero_fill:			/* add null characters if necessary */
	xor	ax,ax
	cmp	cx,*BYTE_LIMIT
	jbe	zero_bytes
	test	di,#1		/* align destination on word boundary */
	jz	zero_words
	stosb
	dec	cx
zero_words:
	shr	cx,#1		/* zero words, not bytes */
	rep
	stos
	adc	cx,cx		/* set up for leftover byte */
zero_bytes:
	rep
	stosb
exit:
	pop	di
	pop	si
	mov	ax,2(bx)
	ret
