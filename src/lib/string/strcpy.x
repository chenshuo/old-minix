/* strcpy.x
 *	char *strcpy(char *s1, const char *s2)
 *
 *	Copy the string pointed to by s2, including the terminating null
 *	character, into the array pointed to by s1.  Returns s1.
 */

.define	_strcpy
.text
_strcpy:
	mov	bx,si		/* save si and di */
	mov	cx,di
	mov	di,sp
	mov	si,4(di)
	mov	di,2(di)
	mov	dx,di
	cld
	test	si,#1		/* align source on word boundary */
	jz	word_copy
	lodsb
	stosb
	orb	al,al
	jz	exit
word_copy:			/* loop to copy words */
	lods
	orb	al,al
	jz	move_last_byte	/* early exit if low byte == 0 */
	stos
	orb	ah,ah
	jnz	word_copy
	jmp	exit
move_last_byte:
	stosb			/* add odd zero byte */
exit:
	mov	ax,dx
	mov	si,bx		/* restore si and di */
	mov	di,cx
	ret
