/* strncmp.x
 *	int strncmp(const char *s1, const char *s2, size_t n)
 *
 *	Compares up to n characters from the strings pointed to by s1
 *	and s2.  Returns zero if the (possibly null terminated) arrays
 *	are identical, a positive number if s1 is greater than s2, and
 *	a negative number otherwise.
 */

.define	_strncmp
.text
_strncmp:
	mov	bx,sp
	push	si
	push	di
	xor	ax,ax		/* default result is equality */
	mov	cx,6(bx)
	jcxz	exit		/* early exit if n == 0 */
	mov	si,2(bx)
	mov	di,4(bx)
	cmp	si,di
	je	exit		/* early exit if s1 == s2 */
	cld
	test	si,#1		/* align s1 on word boundary */
	jz	setup_loop
	lodsb
	orb	al,al
	jz	last_byte_test
	cmpb	al,(di)
	jne	last_byte_test
	xor	ax,ax
	dec	cx
	jz	exit		/* early exit if n == 1 */
	inc	di
setup_loop:
	mov	dx,cx		/* save count */
	shr	cx,#1		/* work with words, not bytes */
	jz	fetch_last_byte
	sub	di,#2		/* set up for faster loop */
word_loop:			/* loop through string by words */
	lods
	add	di,#2
	orb	al,al
	jz	last_byte_test
	cmp	ax,(di)
	jne	find_mismatch
	orb	ah,ah
	loopnz	word_loop
	mov	ax,#0		/* zero return value (without setting flags) */
	jz	exit
	test	dx,#1		/* check for odd byte at end */
	jz	exit
	add	di,#2
fetch_last_byte:
	movb	al,(si)
	jmp	last_byte_test
find_mismatch:			/* check word for mismatched byte */
	cmpb	al,(di)
	jne	last_byte_test
	movb	al,ah
	inc	di
last_byte_test:			/* Expects: (al)=char of s1; (di)->char of s2 */
	xorb	ah,ah
	subb	al,(di)
	sbbb	ah,ah
exit:
	pop	di
	pop	si
	ret
