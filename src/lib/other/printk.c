#include <lib.h>
#include <stdarg.h>

#define MAXDIG		11	/* 32 bits in radix 8 */

#define GETARG(typ)	va_arg(args, typ)

PRIVATE int Xflag;

PRIVATE _PROTOTYPE(char *_itoa, (char *p, unsigned num, int radix));
PRIVATE _PROTOTYPE(char *_ltoa, (char *p, unsigned long num, int radix));
_PROTOTYPE(void putk, (int c));
_PROTOTYPE( void printk, (char *fmt, int arg1));

PRIVATE char *_itoa(p, num, radix)
register char *p;
register unsigned num;
register radix;
{
  register i;
  register char *q;

  q = p + MAXDIG;
  do {
	i = (int) (num % radix);
	i += '0';
	if (i > '9') i += (Xflag ? 'A' : 'a') - '0' - 10;
	*--q = i;
  } while (num = num / radix);
  i = p + MAXDIG - q;
  do
	*p++ = *q++;
  while (--i);
  return(p);
}

PRIVATE char *_ltoa(p, num, radix)
register char *p;
register unsigned long num;
register radix;
{
  register i;
  register char *q;

  q = p + MAXDIG;
  do {
	i = (int) (num % radix);
	i += '0';
	if (i > '9') i += 'A' - '0' - 10;
	*--q = i;
  } while (num = num / radix);
  i = p + MAXDIG - q;
  do
	*p++ = *q++;
  while (--i);
  return(p);
}

void printk(fmt, arg1)
register char *fmt;
int arg1;
{
  char buf[MAXDIG + 1];		/* +1 for sign */
  register int *args = &arg1;
  register char *p, *s;
  int c, i, ndfnd, ljust, lflag, zfill;
  short width, ndigit;
  long l;

  for (;;) {
	c = *fmt++;
	if (c == 0) {
		/* We are done.  Flush the buffer. */
		putk(0);
		return;
	}
	if (c != '%') {
		putk(c);
		continue;
	}
	p = buf;
	s = buf;
	ljust = 0;
	if (*fmt == '-') {
		fmt++;
		ljust++;
	}
	zfill = ' ';
	if (*fmt == '0') {
		fmt++;
		zfill = '0';
	}
	for (width = 0;;) {
		c = *fmt++;
		if (c >= '0' && c <= '9')
			c -= '0';
		else if (c == '*')
			c = GETARG(int);
		else
			break;
		width *= 10;
		width += c;
	}
	ndfnd = 0;
	ndigit = 0;
	if (c == '.') {
		for (;;) {
			c = *fmt++;
			if (c >= '0' && c <= '9')
				c -= '0';
			else if (c == '*')
				c = GETARG(int);
			else
				break;
			ndigit *= 10;
			ndigit += c;
			ndfnd++;
		}
	}
	lflag = 0;
	Xflag = 0;
	if (c == 'l' || c == 'L') {
		lflag++;
		if (*fmt) c = *fmt++;
	}
	switch (c) {
	    case 'X':
		Xflag++;

	    case 'x':
		c = 16;
		goto oxu;

	    case 'U':
		lflag++;

	    case 'u':
		c = 10;
		goto oxu;

	    case 'O':
		lflag++;

	    case 'o':
		c = 8;
  oxu:
		if (lflag) {
			p = _ltoa(p, (unsigned long) GETARG(long), c);
			break;
		}
		p = _itoa(p, (unsigned int) GETARG(int), c);
		break;

	    case 'D':
		lflag++;

	    case 'd':
		if (lflag) {
			if ((l = GETARG(long)) < 0) {
				*p++ = '-';
				l = -l;
			}
			p = _ltoa(p, (unsigned long) l, 10);
			break;
		}
		if ((i = GETARG(int)) < 0) {
			*p++ = '-';
			i = -i;
		}
		p = _itoa(p, (unsigned int) i, 10);
		break;

	    case 'e':
	    case 'f':
	    case 'g':
		zfill = ' ';
		*p++ = '?';
		break;

	    case 'c':
		zfill = ' ';
		*p++ = GETARG(int);
		break;

	    case 's':
		zfill = ' ';
		if ((s = GETARG(char *)) == 0) s = "(null)";
		if (ndigit == 0) ndigit = 32767;
		for (p = s; *p && --ndigit >= 0; p++);
		break;

	    default:	*p++ = c;	break;
	}
	i = p - s;
	if ((width -= i) < 0) width = 0;
	if (ljust == 0) width = -width;
	if (width < 0) {
		if (*s == '-' && zfill == '0') {
			putk( (int) *s);
			s++;
			i--;
		}
		do
			putk(zfill);
		while (++width != 0);
	}
	while (--i >= 0) {
		putk( (int ) *s);
		s++;
	}
	while (width) {
		putk(zfill);
		width--;
	}
  }
}

