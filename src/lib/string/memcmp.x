/* memcmp.x
 *	int memcmp(const void *s1, const void *s2, size_t n)
 *
 *	Compares the first n characters of the objects pointed to by
 *	s1 and s2.  Returns zero if all characters are identical, a
 *	positive number if s1 greater than s2, a negative number otherwise.
 */

#define BYTE_LIMIT 10		/* if n is above this, work with words */

.define	_memcmp
.text
_memcmp:
	mov	bx,sp
	push	si
	push	di
	xor	ax,ax		/* default return is equality */
	mov	cx,6(bx)
	jcxz	exit		/* early exit if n == 0 */
	mov	si,2(bx)
	mov	di,4(bx)
	cmp	si,di
	je	exit		/* early exit if s1 == s2 */
	cld
	cmp	cx,*BYTE_LIMIT
	ja	word_compare
byte_compare:
	repe
	cmpb
	jne	one_past_mismatch
	pop	di
	pop	si
	ret
word_compare:
	test	si,#1		/* align s1 on word boundary */
	jz	word_aligned
	cmpb
	jne	one_past_mismatch
	dec	cx
word_aligned:
	mov	dx,cx		/* save count */
	shr	cx,#1		/* compare words, not bytes */
	jz	almost_done
	repe
	cmp
	je	almost_done
	sub	si,#2
	sub	di,#2
	cmpb
	jne	one_past_mismatch
	jmp	at_mismatch
almost_done:
	test	dx,#1
	jz	exit
	jmp	at_mismatch
one_past_mismatch:
	dec	si
	dec	di
at_mismatch:
	xorb	ah,ah
	movb	al,(si)
	subb	al,(di)
	sbbb	ah,ah
exit:
	pop	di
	pop	si
	ret
