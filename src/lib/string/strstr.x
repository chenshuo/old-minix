/* strstr.x
 *	char * strstr(const char *s1, const char *s2)
 *
 *	Returns a pointer to the first occurrence in the string pointed
 *	to by s1 that is made up of the characters in the string s2.
 */

.define	_strstr
.text
_strstr:
	push	bp
	mov	bp,sp
	sub	sp,#2		/* make room for locals */
	push	si
	push	di
	mov	si,4(bp)
	mov	di,6(bp)
	mov	bx,si		/* default result is s1 */
	movb	ah,(di)		/* fetch first character of s2 */
	orb	ah,ah
	je	exit		/* if s2 is null, we are done */
	cld
	mov	cx,#-1		/* find length of s2 */
	xorb	al,al
	repne
	scasb
	not	cx
	dec	cx
	mov	-2(bp),cx	/* save length of s2 */
	mov	cx,#-1		/* find length + 1 of s1 */
	mov	di,si
	repne
	scasb
	not	cx
	sub	cx,-2(bp)	/* !s1| - |s2| + 1 is number of possibilities */
	jbe	not_found	/* if !s1| < |s2|, give up right now */
	mov	dx,cx
	inc	dx		/* set up for faster loop */
	dec	bx
s1_loop:
	dec	dx
	jz	not_found
	inc	bx
	cmpb	ah,(bx)
	jne	s1_loop		/* no match on first character - try another */
	mov	di,6(bp)
	mov	si,bx
	mov	cx,-2(bp)
	repe
	cmpb
	jne	s1_loop
	jmp	exit
not_found:
	xor	bx,bx
exit:
	mov	ax,bx
	pop	di
	pop	si
	mov	sp,bp
	pop	bp
	ret
