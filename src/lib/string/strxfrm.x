/* strxfrm.x
 *	size_t strxfrm(char *s1, const char *s2, size_t n)
 *
 *	Transforms the string pointed to by s2 into s1.  The effect of
 *	the transformation is to make strcmp() act the same on transformed
 *	strings as strcoll() does on the original strings.  Returns the
 *	length of the transformed string.
 *
 *	Note that this is a BOGUS implementation, since I haven't the
 *	slightest idea what ANSI is prattling about with respect to locale.
 *	It is equivalent to the C code:
 *
 *	int strxfrm(char *s1, char *s2, int n)
 *	{
 *	  strncpy(s1, s2, n);
 *	  return strlen(s2);
 *	}
 */

.define _strxfrm
.text
.extern _strncpy, _strlen
_strxfrm:
	mov	bx,sp		/* quick and dirty call to strncpy() */
	push	6(bx)
	push	4(bx)
	push	2(bx)
	call	_strncpy
	add	sp,#6
	push	4(bx)		/* followed by a call to strlen() */
	call	_strlen
	add	sp,#2
	ret
