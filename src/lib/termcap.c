/*
 *	termcap.c	1.1	20/7/87		agc	Joypace Ltd
 *
 *	Copyright Joypace Ltd, London, UK, 1987. All rights reserved.
 *	This file may be freely distributed provided that this notice
 *	remains attached.
 *
 *	A public domain implementation of the termcap(3) routines.
 */

/*    efth      1988-Apr-29

    - Correct when TERM != name and TERMCAP is defined   [tgetent]
    - Correct the comparison for the terminal name       [tgetent]
    - Correct the value of ^x escapes                    [tgetstr]
    - Added %r to reverse row/column			 [tgoto]
*/

#include <stdio.h>

#define CAPABLEN	2

#define ISSPACE(c)	((c) == ' ' || (c) == '\t' || (c) == '\r' || (c) == '\n')
#define ISDIGIT(x)	((x) >= '0' && (x) <= '9')

char		*capab;		/* the capability itself */

extern char	*getenv();	/* new, improved getenv */
extern FILE	*fopen();	/* old fopen */

/*
 *	tgetent - get the termcap entry for terminal name, and put it
 *	in bp (which must be an array of 1024 chars). Returns 1 if
 *	termcap entry found, 0 if not found, and -1 if file not found.
 */
int
tgetent(bp, name)
char	*bp;
char	*name;
{
	FILE	*fp;
	char	*file;
	char	*cp;
	short	len = strlen(name);

	capab = bp;

	/* Fixed problem: If TERM != name and TERMCAP was defined,  */
	/* then should look in /etc/termcap, but didn't.  (efth)    */

	if ( (file = getenv("TERMCAP")) == NULL )
	    file = "/etc/termcap";
	else if ( *file != '/' )
	    if ( (cp = getenv("TERM")) != NULL && strcmp( name, cp ) == 0 ) {
		strcpy( bp, file );
		return( 1 );
	    } else
		file = "/etc/termcap";


	if ((fp = fopen(file, "r")) == (FILE *) NULL)
		return(-1); 

	while (fgets(bp, 1024, fp) != NULL) {
		/*  Read in the rest of the definition now  (efth)  */
		while (*(cp = &bp[strlen(bp) - 2]) == '\\')
			fgets(cp, 1024, fp);
		
		/* skip V6 two letter name */
		for (cp = bp ; *cp != '|' ; cp++)
			;
		for (++cp ; ISSPACE(*cp) ; cp++)
			;

		/*  Make sure "name" matches exactly  (efth)  */
		if (strncmp(name, cp, len) == 0  &&  cp[len] == '|') {
			fclose(fp);
			return(1);
		}
	}
	fclose(fp);
	return(0);
	
}

/*
 *	tgetnum - get the numeric terminal capability corresponding
 *	to id. Returns the value, -1 if invalid.
 */
int
tgetnum(id)
char	*id;
{
	char	*cp;
	int	ret;

	if ((cp = capab) == NULL || id == NULL)
		return(-1);
	while (*++cp != ':')
		;
	for (++cp ; *cp ; cp++) {
		while (ISSPACE(*cp))
			cp++;
		if (strncmp(cp, id, CAPABLEN) == 0) {
			while (*cp && *cp != ':' && *cp != '#')
				cp++;
			if (*cp != '#')
				return(-1);
			for (ret = 0, cp++ ; *cp && ISDIGIT(*cp) ; cp++)
				ret = ret * 10 + *cp - '0';
			return(ret);
		}
		while (*cp && *cp != ':')
			cp++;
	}
	return(-1);
}

/*
 *	tgetflag - get the boolean flag corresponding to id. Returns -1
 *	if invalid, 0 if the flag is not in termcap entry, or 1 if it is
 *	present.
 */
int
tgetflag(id)
char	*id;
{
	char	*cp;

	if ((cp = capab) == NULL || id == NULL)
		return(-1);
	while (*++cp != ':')
		;
	for (++cp ; *cp ; cp++) {
		while (ISSPACE(*cp))
			cp++;
		if (strncmp(cp, id, CAPABLEN) == 0)
			return(1);
		while (*cp && *cp != ':')
			cp++;
	}
	return(0);
}

/*
 *	tgetstr - get the string capability corresponding to id and place
 *	it in area (advancing area at same time). Expand escape sequences
 *	etc. Returns the string, or NULL if it can't do it.
 */
char *
tgetstr(id, area)
char	*id;
char	**area;
{
	char	*cp;
	char	*ret;
	int	i;

	if ((cp = capab) == NULL || id == NULL)
		return(NULL);
	while (*++cp != ':')
		;
	for (++cp ; *cp ; cp++) {
		while (ISSPACE(*cp))
			cp++;
		if (strncmp(cp, id, CAPABLEN) == 0) {
			while (*cp && *cp != ':' && *cp != '=')
				cp++;
			if (*cp != '=')
				return(NULL);
			for (ret = *area, cp++; *cp && *cp != ':' ; (*area)++, cp++)
				switch(*cp) {
				case '^' :
					**area = *++cp - '@'; /* fix (efth)*/
					break;
				case '\\' :
					switch(*++cp) {
					case 'E' :
						**area = '\033';
						break;
					case 'n' :
						**area = '\n';
						break;
					case 'r' :
						**area = '\r';
						break;
					case 't' :
						**area = '\t';
						break;
					case 'b' :
						**area = '\b';
						break;
					case 'f' :
						**area = '\f';
						break;
					case '0' :
					case '1' :
					case '2' :
					case '3' :
						for (i=0 ; *cp && ISDIGIT(*cp) ; cp++)
							i = i * 8 + *cp - '0';
						**area = i;
						cp--;
						break;
					case '^' :
					case '\\' :
						**area = *cp;
						break;
					}
					break;
				default :
					**area = *cp;
				}
			*(*area)++ = '\0';
			return(ret);
		}
		while (*cp && *cp != ':')
			cp++;
	}
	return(NULL);
}

/*
 *	tgoto - given the cursor motion string cm, make up the string
 *	for the cursor to go to (destcol, destline), and return the string.
 *	Returns "OOPS" if something's gone wrong, or the string otherwise.
 */
char *
tgoto(cm, destcol, destline)
char	*cm;
int	destcol;
int	destline;
{
	register char	*rp;
	static char	ret[24];
	int		incr = 0;
	int 		argno = 0, numval;

	for (rp = ret ; *cm ; cm++) {
		switch(*cm) {
		case '%' :
			switch(*++cm) {
			case '+' :
				numval = (argno == 0 ? destline : destcol);
				argno = 1 - argno;
				*rp++ = numval + incr + *++cm;
				break;

			case '%' :
				*rp++ = '%';
				break;

			case 'i' :
				incr = 1;
				break;

			case 'd' :
				numval = (argno == 0 ? destline : destcol);
				numval += incr;
				argno = 1 - argno;
				*rp++ = '0' + (numval/10);
				*rp++ = '0' + (numval%10);
				break;

			case 'r' :
				argno = 1;
				break;
			}

			break;
		default :
			*rp++ = *cm;
		}
	}
	*rp = '\0';
	return(ret);
}

/*
 *	tputs - put the string cp out onto the terminal, using the function
 *	outc. This should do padding for the terminal, but I can't find a
 *	terminal that needs padding at the moment...
 */
int
tputs(cp, affcnt, outc)
register char	*cp;
int		affcnt;
int		(*outc)();
{
	if (cp == NULL)
		return(1);
	/* do any padding interpretation - left null for MINIX just now */
	while (*cp)
		(*outc)(*cp++);
	return(1);
}

/*
 *	That's all, folks...
 */
