/* memset.x
 *	void *memset(void *s, int c, size_t n)
 *
 *	Copies the value of c (converted to unsigned char) into the
 *	first n locations of the object pointed to by s.
 */

#ifdef i80386
#define BYTE_LIMIT	16	/* if n is above this, work with doublewords */
#define SIZE_OVERRIDE	.byte 102 /* force 32 bits */
#define SHLAX(n)	.byte 193,224,n
#define SHRCX(n)	.byte 193,233,n
#else
#define BYTE_LIMIT	10	/* if n is above this, work with words */
#endif

.define	_memset
.text
_memset:
	push	di
	mov	di,sp
	mov	cx,8(di)
	movb	al,6(di)
	mov	di,4(di)
	mov	bx,di		/* return value is s */
	jcxz	exit		/* early exit if n == 0 */
	cld
	cmp	cx,*BYTE_LIMIT
	jbe	byte_set
	movb	ah,al		/* set up second byte */
	test	di,#1		/* align on word boundary */
	jz	word_aligned
	stob
	dec	cx
word_aligned:
#ifdef i80386
	test	di,#2		/* align on doubleword boundary */
	jz	dword_aligned
	stow
	sub	cx,*2
dword_aligned:
	mov	dx,ax		/* duplicate byte in all bytes of EAX */
	SIZE_OVERRIDE
	SHLAX	(16)
	mov	ax,dx
	mov	dx,cx		/* save count */
	SHRCX	(2)
	rep
	SIZE_OVERRIDE
	stow
	and	dx,#3		/* set up to set leftover bytes */
	mov	cx,dx
#else
	shr	cx,#1		/* set words, not bytes */
	rep
	stow
	adc	cx,cx		/* set up to set leftover byte */
#endif
byte_set:
	rep
	stob
exit:
	pop	di
	mov	ax,bx
	ret
