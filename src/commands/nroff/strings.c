/* strings.c - String input/output processing for nroff word processor */

#include <stdio.h>
#include "nroff.h"



/*------------------------------*/
/*	defstr			*/
/*------------------------------*/
void defstr(p)
register char *p;
{

/*
 *	Define a string. top level, read from command line.
 *
 *	we should read string without interpretation EXCEPT:
 *
 *	1) number registers are interpolated
 *	2) strings indicated by \* are interpolated
 *	3) arguments indicated by \$ are interpolated
 *	4) concealed newlines indicated by \(newline) are eliminated
 *	5) comments indicated by \" are eliminated
 *	6) \t and \a are interpreted as ASCII h tab and SOH.
 *	7) \\ is interpreted as backslash and \. is interpreted as a period.
 *
 *	currently, we do only 3. a good place to do it would be here before
 *	putstr, after colstr...
 */

  register char *q;
  register int i;
  char name[MNLEN];
  char defn[MXMLEN];



  name[0] = '\0';
  defn[0] = '\0';


  /* Skip the .ds and get to the name... */
  q = skipwd(p);
  q = skipbl(q);

  /* Ok, name now holds the name. make sure it is valid (i.e. first
   * char is alpha...). getwrd returns the length of the word. */
  i = getwrd(q, name);
  if (!name[0]) {
	fprintf(err_stream,
		"***%s: missing or illegal string definition name\n",
		myname);
	err_exit(-1);
  }

  /* Truncate to 2 char max name. */
  if (i > 2) name[2] = EOS;


  /* Skip the name to get to the string. it CAN start with a " to have
   * leading blanks... */
  q = skipwd(q);
  q = skipbl(q);



  /* Read rest of line from input stream and collect string into temp
   * buffer defn */
  if ((i = colstr(q, defn)) == ERR) {
	fprintf(err_stream,
		"***%s: string definition too long\n", myname);
	err_exit(-1);
  }

  /* Store the string */
  if (putstr(name, defn) == ERR) {
	fprintf(err_stream,
		"***%s: string definition table full\n", myname);
	err_exit(-1);
  }
}





/*------------------------------*/
/*	colstr			*/
/*------------------------------*/
int colstr(p, d)
register char *p;
char *d;
{

/*
 *	Collect string definition from input stream
 */

  register int i = 0;

  if (*p == '\"') p++;

  while (*p != EOS) {
	if (i >= MXMLEN - 1) {
		d[i - 1] = EOS;
		return(ERR);
	}
	d[i++] = *p++;
  }
  d[i] = EOS;
  return(i);
}





/*------------------------------*/
/*	putstr			*/
/*------------------------------*/
int putstr(name, p)
register char *name;
register char *p;
{

/*
 *	Put string definition into (macro) table
 *
 *	NOTE: any expansions of things like number registers SHOULD
 *	have been done already. strings and macros share mb buffer
 */


  /* Any room left? (did we exceed max number of possible macros) */
  if (mac.lastp >= MXMDEF) return(ERR);

  /* Will new one fit in big buffer? */
  if (mac.emb + strlen(name) + strlen(p) + 1 > &mac.mb[MACBUF]) {
	return(ERR);
  }

  /* Add it... 
   * bump counter, set ptr to name, copy name, copy def. finally increment
   * end of macro buffer ptr (emb).
   * 
   * string looks like this in mb:
   * 
   * mac.mb[MACBUF]		size of total buf lastp < MXMDEF
   * umber of macros/strings possible *mnames[MXMDEF]		->
   * names, each max length
   * ...______________________________...____________________... / /
   * /|X|X|0|string definition      |0| / / / / / / /
   * .../_/_/_|_|_|_|_________________...___|_|/_/_/_/_/_/_/_... ^ |
   * \----- mac.mnames[mac.lastp] points here
   * 
   * both the 2 char name (XX) and the descripton are null term and follow
   * one after the other. */
  ++mac.lastp;
  mac.mnames[mac.lastp] = mac.emb;
  strcpy(mac.emb, name);
  strcpy(mac.emb + strlen(name) + 1, p);
  mac.emb += strlen(name) + strlen(p) + 2;
  return(OK);
}






/*------------------------------*/
/*	getstr			*/
/*------------------------------*/
char *getstr(name)
register char *name;
{

/*
 *	Get (lookup) string definition from namespace
 */

  register int i;

  /* Loop for all macros, starting with last one */
  for (i = mac.lastp; i >= 0; --i) {
	/* Is this REALLY a macro? */
	if (mac.mnames[i]) {
		/* If it compares, return a ptr to it */
		if (!strcmp(name, mac.mnames[i])) {
/*!!!debug			puts (mac.mnames[i]);*/

			if (mac.mnames[i][1] == EOS)
				return(mac.mnames[i] + 2);
			else
				return(mac.mnames[i] + 3);
		}
	}
  }

  /* None found, return null */
  return(NULL_CPTR);
}
