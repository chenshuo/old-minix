/* strcoll.x
 *	int strcoll(const char *s1, const char *s2)
 *
 *	Compares the strings pointed to by s1 and s2, in light of the
 *	current locale.  Returns zero if the strings are identical, a
 *	positive number if s1 is greater than s2, and a negative number
 *	otherwise.
 *
 *	Note that this is a BOGUS implementation, since I haven't the
 *	slightest idea what ANSI is prattling about with respect to locale.
 */

.define _strcoll
.text
.extern _strcmp
_strcoll:
	jmp	_strcmp
