/* strrchr.x
 *	char *strrchr(const char *s, int c)
 *
 *	Locates final occurrence of c (as unsigned char) in string s.
 */

.define	_strrchr
.text
_strrchr:
	mov	bx,di		/* save di */
	mov	di,sp
	xor	dx,dx		/* default result is NULL */
	movb	ah,4(di)
	mov	di,2(di)
	cld
	mov	cx,#-1		/* find end of string */
	xorb	al,al
	repne
	scasb
	not	cx		/* silly trick gives length (including null) */
	dec	di		/* point back at null character */
	movb	al,ah		/* find last occurrence of c */
	std
	repne
	scasb
	jne	exit
#ifdef i8088
	inc	di
	mov	dx,di
#else
	lea	dx,1(di)
#endif
exit:
	cld			/* clear direction flag */
	mov	di,bx		/* restore di */
	mov	ax,dx
	ret
