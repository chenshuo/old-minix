/* strtok.x
 *	char *strtok(char *s1, const char *s2)
 *
 *	Returns a pointer to the "next" token in s1.  Tokens are 
 *	delimited by the characters in the string pointed to by s2.
 */

.define	_strtok
.data
scan:	.data2	0
.text
_strtok:
	push	bp
	mov	bp,sp
	push	si
	push	di
	cld
	mov	bx,4(bp)
	or	bx,bx		/* if s ~= NULL, */
	jnz	s2_length	/*   we start a new string */
	mov	bx,scan
	or	bx,bx		/* if old string exhausted, */
	jz	exit		/*   exit early */
s2_length:			/* find length of s2 */
	mov	di,6(bp)
	mov	cx,#-1
	xorb	al,al
	repne
	scasb
	not	cx
	dec	cx
	jz	string_finished	/* if s2 has length zero, we are done */
	mov	dx,cx		/* save length of s2 */

	mov	si,bx
	xor	bx,bx		/* return value is NULL */
delim_loop:			/* dispose of leading delimiters */
	lodsb
	orb	al,al
	jz	string_finished
	mov	di,6(bp)
	mov	cx,dx
	repne
	scasb
	je	delim_loop

	lea	bx,-1(si)	/* return value is start of token */
token_loop:			/* find end of token */
	lodsb
	orb	al,al
	jz	string_finished
	mov	di,6(bp)
	mov	cx,dx
	repne
	scasb
	jne	token_loop
	movb	-1(si),*0	/* terminate token */
	mov	scan,si		/* set up for next call */
	jmp	exit
string_finished:
	mov	scan,#0		/* ensure NULL return in future */
exit:
	mov	ax,bx
	pop	di
	pop	si
	mov	sp,bp
	pop	bp
	ret
