/* strcmp.x
 *	int strcmp(const char *s1, const char *s2)
 *
 *	Compares the strings pointed to by s1 and s2.  Returns zero if
 *	strings are identical, a positive number if s1 greater than s2,
 *	and a negative number otherwise.
 */

.define	_strcmp
.text
_strcmp:
	mov	bx,si		/* save si and di */
	mov	cx,di
	mov	di,sp
	mov	si,2(di)
	mov	di,4(di)
	xor	ax,ax		/* default return is equality */
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
	inc	di
setup_loop:
	sub	di,#2		/* set up for faster loop */
word_loop:			/* loop through string by words */
	lods
	add	di,#2
	orb	al,al
	jz	last_byte_test
	cmp	ax,(di)
	jne	find_mismatch
	orb	ah,ah
	jnz	word_loop
	xor	ax,ax
	jmp	exit
find_mismatch:
	cmpb	al,(di)
	jne	last_byte_test
	movb	al,ah
	inc	di
last_byte_test:			/* Expects: (al)=char of s1; (di)->char of s2 */
	xorb	ah,ah
	subb	al,(di)
	sbbb	ah,ah
exit:
	mov	si,bx		/* restore si and di */
	mov	di,cx
	ret
