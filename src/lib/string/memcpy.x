/* memcpy.s */
/*	void *memcpy(void *s1, const void *s2, size_t n); */

/*	Copy n characters from the object pointed to by s2 into the */
/*	object pointed to by s1.  If the objects overlap, the result */
/*	is undefined. */

/*	Optimized for the common case of s being a large word-aligned */
/*	buffer, of even length.  Compared to the old code, there are */
/*	slight time penalties for unaligned buffers and those of odd */
/*	length.  There is also a slight time penalty for n < 4, and a */
/*	serious time penalty for s1 == s2 --- neither is very common. */

.extern	_memcpy
.text
_memcpy:
	mov	bx,si		/* save si and di */
	mov	dx,di
	mov	di,sp
	mov	cx,6(di)
	mov	si,4(di)
	mov	di,2(di)
	mov	ax,di		/* save a copy of s1 */
	cld
	test	si,#1		/* if not word aligned */
	jnz	align_for_words	/*   go align things */
	shr	cx,#1		/* move words, not bytes */
	rep	movsw
	jb	move_last_byte	/* if not even words, go move the last one */
	mov	si,bx		/* restore si and di */
	mov	di,dx
	ret
align_for_words:
	jcxz	exit		/* n == 0: early exit */
	movsb
	dec	cx
	shr	cx,#1		/* following code replicated for speed */
	rep	movsw
	jb	move_last_byte
	mov	si,bx
	mov	di,dx
	ret
move_last_byte:
	movsb
exit:
	mov	si,bx		/* restore si and di */
	mov	di,dx
	ret
