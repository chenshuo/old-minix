/* strcat.x
 *	char *strcat(char *s1, const char *s2)
 *
 *	Concatenates the string pointed to by s2 onto the end of the
 *	string pointed to by s1.  Returns s1.
 */

.define _strcat
.text
_strcat:
	mov	bx,si		/* save si and di */
	mov	dx,di
	mov	si,sp
	mov	di,2(si)
	push	di		/* save return value */
	mov	si,4(si)
	cmpb	(si),*0
	je	exit		/* early exit if s2 is the null string */
	cld
	mov	cx,#-1		/* find end of s1 */
	xorb	al,al
	repne
	scab
	dec	di		/* point back at null character */
	test	si,#1		/* align source on word boundary */
	jz	word_copy
	movb
word_copy:			/* loop to copy words */
	lodw
	orb	al,al
	jz	move_last_byte	/* exit if low byte == 0 */
	stow
	orb	ah,ah
	jnz	word_copy
	jmp	exit
move_last_byte:
	stob			/* add odd zero byte */
exit:
	mov	si,bx
	mov	di,dx
	pop	ax
	ret
