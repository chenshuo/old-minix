/* io.c - low level I/O processing portion of nroff word processor */

#include <stdio.h>
#include "nroff.h"

/*------------------------------*/
/*	getlin			*/
/*------------------------------*/
int getlin(p, in_buf)
char *p;
FILE *in_buf;
{

/*
 *	retrieve one line of input text
 */

  register char *q;
  register int i;
  int c;
  int nreg;

  q = p;
  for (i = 0; i < MAXLINE - 1; ++i) {
	c = ngetc(in_buf);
	if (c == EOF) {
		*q = EOS;
		c = strlen(p);
		return(c == 0 ? EOF : c);
	}
	*q++ = c;
	if (c == '\n') break;
  }
  *q = EOS;

  nreg = findreg(".c");
  if (nreg > 0) set_ireg(".c", rg[nreg].rval + 1, 0);

  return(strlen(p));
}






/*------------------------------*/
/*	ngetc			*/
/*------------------------------*/
int ngetc(infp)
FILE *infp;
{

/*
 *	get character from input file or push back buffer
 */

  register int c;

  if (mac.ppb >= &mac.pbb[0])
	c = *mac.ppb--;
  else
	c = getc(infp);

  return(c);
}



/*------------------------------*/
/*	pbstr			*/
/*------------------------------*/
void pbstr(p)
char *p;
{

/*
 *	Push back string into input stream
 */

  register int i;

  /* If string is null, we do nothing */
  if (p == NULL_CPTR) return;
  if (p[0] == EOS) return;
  for (i = strlen(p) - 1; i >= 0; --i) {
	putbak(p[i]);
  }
}





/*------------------------------*/
/*	putbak			*/
/*------------------------------*/
void putbak(c)
char c;
{

/*
 *	Push character back into input stream. we use the push-back buffer
 *	stored with macros.
 */

  if (mac.ppb < &(mac.pbb[0])) {
	mac.ppb = &(mac.pbb[0]);
	*mac.ppb = c;
  } else {
	if (mac.ppb >= &mac.pbb[MAXLINE - 1]) {
		fprintf(err_stream,
		      "***%s: push back buffer overflow\n", myname);
		err_exit(-1);
	}
	*++(mac.ppb) = c;
  }
}



/*------------------------------*/
/*	prchar			*/
/*------------------------------*/
void prchar(c, fp)
char c;
FILE *fp;
{

/*
 *	print character with test for printer
 */

  if (fp == stdout)
	putc(c, fp);
  else
	putc_lpr(c, fp);
}






/*------------------------------*/
/*	put			*/
/*------------------------------*/
void put(p)
char *p;
{

/*
 *	put out line with proper spacing and indenting
 */

  register int j;
  char os[MAXLINE];

  if (pg.lineno == 0 || pg.lineno > pg.bottom) {
	phead();
  }
  if (dc.prflg == TRUE) {
	if (!dc.bsflg) {
		if (strkovr(p, os) == TRUE) {
			for (j = 0; j < pg.offset; ++j)
				prchar(' ', out_stream);
			for (j = 0; j < dc.tival; ++j)
				prchar(' ', out_stream);
			putlin(os, out_stream);
		}
	}
	for (j = 0; j < pg.offset; ++j) prchar(' ', out_stream);
	for (j = 0; j < dc.tival; ++j) prchar(' ', out_stream);
	putlin(p, out_stream);
  }
  dc.tival = dc.inval;
  skip(min(dc.lsval - 1, pg.bottom - pg.lineno));
  pg.lineno = pg.lineno + dc.lsval;
  set_ireg("ln", pg.lineno, 0);
  if (pg.lineno > pg.bottom) {
	pfoot();
	if (stepping) wait_for_char();
  }
}




/*------------------------------*/
/*	putlin			*/
/*------------------------------*/
void putlin(p, pbuf)
register char *p;
FILE *pbuf;
{

/*
 *	output a null terminated string to the file
 *	specified by pbuf.
 */

  while (*p != EOS) prchar(*p++, pbuf);
}




/*------------------------------*/
/*	putc_lpr		*/
/*------------------------------*/
void putc_lpr(c, fp)
char c;
FILE *fp;
{

/*
 *	write char to printer
 */

  putc(c, fp);
}
