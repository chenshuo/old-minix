/* command.c - command input parser/processor for nroff text processor */

#include <stdio.h>
#include "nroff.h"

/*------------------------------*/
/*	comand			*/
/*------------------------------*/
void comand(p)
register char *p;
{
/* Main command processor */
  register int i;
  register int ct;
  register int val;
  register int indx;
  int spval;
  char argtyp;
  char name[MAXLINE];
  char macexp[MXMLEN];
  int tmp;
  char *pfs;
  char fs[20];

  /* Get command code */
  ct = comtyp(p, macexp);

  /* Error? */
  if (ct == UNKNOWN) {
	fprintf(err_stream, "***%s: unrecognized command %s\n", myname, p);
	return;
  }

  /* Ignore comments */
  if (ct == COMMENT) return;

  /* Do escape expansion on command line args */
  expesc(p, name);

  /* Get value of command */
  val = getval(p, &argtyp);

  /* Do the command */
  switch (ct) {
	/* Set (&param, val, type, defval, minval, maxval) */
      case FC:
	/* Field delim/pad chars 
	 * .fc [delim] [pad] */
	fprintf(err_stream, "***%s: .fc not available\n", myname);
	break;

      case TR:
	/* Translate: .tr ab... */
	fprintf(err_stream, "***%s: .tr not available\n", myname);
	break;

      case AD:
	/* Adjust:  .ad [mode] */
	p = skipwd(p);
	p = skipbl(p);

	switch (*p) {
	    case 'l':
		dc.adjval = ADJ_LEFT;
		dc.juval = YES;
		break;
	    case 'r':
		dc.adjval = ADJ_RIGHT;
		dc.juval = YES;
		break;
	    case 'c':
		dc.adjval = ADJ_CENTER;
		dc.juval = YES;
		break;
	    case 'b':
	    case 'n':
		dc.adjval = ADJ_BOTH;
		dc.juval = YES;
		break;
	    default:	break;
	}
	break;

      case AF:
	/* Assign format to number reg:  .af R {1,a,A,i,I,0...1} */
	p = skipwd(p);
	p = skipbl(p);
	if (!isalpha(*p)) {
		fprintf(err_stream,
		 "***%s: invalid or missing number register name\n", myname);
	} else {
		/* Number register format is 1,a,A,i,I,0...1 default
		 * is 1. for 0001 format, store num dig or'ed with
		 * 0x80, up to 8 digits. 
		 */
		indx = tolower(*p) - 'a';
		p = skipwd(p);
		p = skipbl(p);
		if (*p == '1')
			dc.nrfmt[indx] = '1';
		else if (*p == 'a')
			dc.nrfmt[indx] = 'a';
		else if (*p == 'A')
			dc.nrfmt[indx] = 'A';
		else if (*p == 'i')
			dc.nrfmt[indx] = 'i';
		else if (*p == 'I')
			dc.nrfmt[indx] = 'I';
		else if (*p == '0') {
			for (i = 0; isdigit(p[i]); i++);
			dc.nrfmt[indx] = (char) (i);
			if (dc.nrfmt[indx] <= 0)
				dc.nrfmt[indx] = '1';
			else if (dc.nrfmt[indx] > 8) {
				dc.nrfmt[indx] = 8;
				dc.nrfmt[indx] |= 0x80;
			} else
				dc.nrfmt[indx] |= 0x80;

		} else
			dc.nrfmt[indx] = '1';
	}
	break;

      case BD:
	/* Embolden font (IGNORED):  .bd [S] F N */
	break;

      case BO:
	/* Bold face: .bo [N] */
	set(&dc.boval, val, argtyp, 1, 0, HUGE);
	dc.cuval = dc.ulval = 0;
	break;

      case BP:
	/* Begin page: .bp [+/-N] */
	if (pg.lineno > 0) space(HUGE);
	set(&pg.curpag, val, argtyp, pg.curpag + 1, -HUGE, HUGE);
	pg.newpag = pg.curpag;
	set_ireg("%", pg.newpag, 0);
	break;

      case BR:
	/* Break (page):  .br */
	robrk();
	break;

      case BS:
	/* Backspace in output:  .bs [N] */
	set(&dc.bsflg, val, argtyp, 1, 0, 1);
	break;

      case C2:
	/* Nobreak char:  .c2 [c='] */
	if (argtyp == '\r' || argtyp == '\n')
		dc.nobrchr = '\'';
	else
		dc.nobrchr = argtyp;
	break;

      case CC:
	/* Command character:  .cc [c=.] */
	if (argtyp == '\r' || argtyp == '\n')
		dc.cmdchr = '.';
	else
		dc.cmdchr = argtyp;
	break;

      case CE:
	/* Center:  .ce [N] */
	robrk();
	set(&dc.ceval, val, argtyp, 1, 0, HUGE);
	break;

      case CS:
	/* Constant space char (IGNORED):  .cs F N M */
	break;

      case CU:
	/* Continuous underline:  .cu [N] */
	set(&dc.cuval, val, argtyp, 1, 0, HUGE);
	dc.ulval = dc.boval = 0;
	break;

      case DE: 
	/* Define macro:  .de name [end] */
	ignoring = FALSE;
	defmac(p, sofile[dc.flevel]);
	break;

      case DS:
	/* Define string:  .ds name string */
	defstr(p);
	break;

      case EC:
	/* Escape char:  .ec [c=\] */
	if (argtyp == '\r' || argtyp == '\n')
		dc.escchr = '\\';
	else
		dc.escchr = argtyp;
	dc.escon = YES;
	break;

      case EF:
	/* Even footer: .ef "a" "b" "c" */
	gettl(p, pg.efoot, &pg.eflim[0]);
	break;

      case EH:
	/* Even header:  .eh "a" "b" "c" */
	gettl(p, pg.ehead, &pg.ehlim[0]);
	break;

      case EN:
	/* End macro def (should not get one here...):  .en or .. */
	fprintf(err_stream, "***%s: missing .de command\n", myname);
	break;

      case EO:
	/* Escape off:  .eo */
	dc.escon = NO;
	break;

      case FI:
	/* Fill:  .fi */
	robrk();
	dc.fill = YES;
	break;

      case FL:
	/* Flush NOW:  .fl */
	fflush(out_stream);
	break;

      case FO:
	/* Footer: .fo "a" "b" "c" */
	gettl(p, pg.efoot, &pg.eflim[0]);
	gettl(p, pg.ofoot, &pg.oflim[0]);
	break;

      case FT:
	/* Font change:  .ft {R,I,B,S,P}
	 * 
	 * The way it's implemented here, it causes a break rather than
	 * be environmental... */
	p = skipwd(p);
	p = skipbl(p);
	if (!isalpha(*p)) {
		fprintf(err_stream,
			"***%s: invalid or missing font name\n",
			myname);
	} else {
		pfs = &fs[0];

		fontchange(*p, pfs);

		robrk();
		fflush(out_stream);
		fprintf(out_stream, "%s", pfs);
		fflush(out_stream);
	}
	break;

      case TL:
      case HE:
	/* Header (both are currently identical. .he is -me) 
	 * .tl "a" "b" "c" .he "a" "b" "c" 
	 */
	gettl(p, pg.ehead, &pg.ehlim[0]);
	gettl(p, pg.ohead, &pg.ohlim[0]);
	break;

      case IG:
	/* Ignore input lines:  .ig name */
	ignoring = TRUE;
	defmac(p, sofile[dc.flevel]);
	break;

      case IN:
	/* Indenting:  .in [+/-N] */
	set(&dc.inval, val, argtyp, 0, 0, dc.rmval - 1);
	set_ireg(".i", dc.inval, 0);
	dc.tival = dc.inval;
	break;

      case JU:
	/* Justify:  .ju */
	dc.juval = YES;
	break;

      case LL:
	/* Line length:  .ll [+/-N] .rm [+/-N] */
	set(&dc.rmval, val, argtyp, PAGEWIDTH, dc.tival + 1, HUGE);
	set(&dc.llval, val, argtyp, PAGEWIDTH, dc.tival + 1, HUGE);
	set_ireg(".l", dc.llval, 0);
	break;

      case LS:
	/* Line spacing:  .ls [+/-N=+1] */
	set(&dc.lsval, val, argtyp, 1, 1, HUGE);
	set_ireg(".v", dc.lsval, 0);
	break;

      case LT:
	/* Title length:  .lt N */
	set(&dc.ltval, val, argtyp, PAGEWIDTH, 0, HUGE);
	pg.ehlim[RIGHT] = dc.ltval;
	pg.ohlim[RIGHT] = dc.ltval;
	break;

      case M1:
	/* Topmost margin:  .m1 N */
	set(&pg.m1val, val, argtyp, 2, 0, HUGE);
	break;

      case M2:
	/* Second top margin:  .m2 N */
	set(&pg.m2val, val, argtyp, 2, 0, HUGE);
	break;

      case M3:
	/* 1st bottom margin:  .m3 N */
	set(&pg.m3val, val, argtyp, 2, 0, HUGE);
	pg.bottom = pg.plval - pg.m4val - pg.m3val;
	break;

      case M4:
	/* Bottom-most margin: .m4 N */
	set(&pg.m4val, val, argtyp, 2, 0, HUGE);
	pg.bottom = pg.plval - pg.m4val - pg.m3val;
	break;

      case MACRO:
	/* Macro expansion:  (internal) */
	maceval(p, macexp);
	break;

      case NA:
	/* No adjust:  .na */
	dc.adjval = ADJ_OFF;
	dc.juval = NO;
	break;

      case NE:
	/* Need n lines:  .ne N */
	robrk();
	if ((pg.bottom - pg.lineno + 1) < (val * dc.lsval)) {
		space(HUGE);
	}
	break;

      case NF:
	/* No fill:  .nf */
	robrk();
	dc.fill = NO;
	break;

      case NJ:
	/* No justify: .nj */
	dc.juval = NO;
	break;

      case NR:
	/* Set number reg:  .nr R +/-N M */
	p = skipwd(p);
	p = skipbl(p);
	if (!isalpha(*p)) {
		fprintf(err_stream,
		 "***%s: invalid or missing number register name\n",
			myname);
	} else {
		/* Indx is the register, R, and val is the final
		 * value (default = 0). getval does skipwd,skipbl */
		indx = tolower(*p) - 'a';
		val = getval(p, &argtyp);
		set(&dc.nr[indx], val, argtyp, 0, -INFINITE, INFINITE);

		/* Now get autoincrement M, if any (default = 1).
		 * getval does skipwd,skipbl */
		p = skipwd(p);
		p = skipbl(p);
		val = getval(p, &argtyp);
		set(&dc.nrauto[indx], val, '1', 1, -INFINITE, INFINITE);
	}
	break;

      case OF:
	/* Odd footer:  .of "a" "b" "c" */
	gettl(p, pg.ofoot, &pg.oflim[0]);
	break;

      case OH:
	/* Odd header:  .oh "a" "b" "c" */
	gettl(p, pg.ohead, &pg.ohlim[0]);
	break;

      case PC:
	/* Page number char:  .pc [c=NULL] */
	if (argtyp == '\r' || argtyp == '\n')
		dc.pgchr = EOS;
	else
		dc.pgchr = argtyp;
	break;

      case PL:
	/* Page length: .pl N */
	set(&pg.plval,
	    val,
	    argtyp,
	    PAGELEN,
	    pg.m1val + pg.m2val + pg.m3val + pg.m4val + 1,
	    HUGE);
	set_ireg(".p", pg.plval, 0);
	pg.bottom = pg.plval - pg.m3val - pg.m4val;
	break;

      case PM:
	/* Print macro names and sizes:  .pm [t] */
	if (argtyp == '\r' || argtyp == '\n')
		printmac(0);
	else if (argtyp == 't')
		printmac(1);
	else if (argtyp == 'T')
		printmac(2);
	else
		printmac(0);
	break;

      case PN:
	/* Page number:  .pn N */
	tmp = pg.curpag;
	set(&pg.curpag, val - 1, argtyp, tmp, -HUGE, HUGE);
	pg.newpag = pg.curpag + 1;
	set_ireg("%", pg.newpag, 0);
	break;

      case PO:
	/* Page offset:  .po N */
	set(&pg.offset, val, argtyp, 0, 0, HUGE);
	set_ireg(".o", pg.offset, 0);
	break;

      case PS:
	/* Point size (IGNORED):  .ps +/-N */
	break;

      case RR:
	/* Unset number reg:  .rr R */
	p = skipwd(p);
	p = skipbl(p);
	if (!isalpha(*p)) {
		fprintf(err_stream,
		 "***%s: invalid or missing number register name\n", myname);
	} else {
		indx = tolower(*p) - 'a';
		val = 0;
		set(&dc.nr[indx], val, argtyp, 0, -HUGE, HUGE);
	}
	break;

      case SO:
	/* Source file:  .so name */
	p = skipwd(p);
	p = skipbl(p);
	if (getwrd(p, name) == 0) break;

	if (dc.flevel + 1 >= Nfiles) {
		fprintf(err_stream,
			"***%s: .so commands nested too deeply\n",
			myname);
		err_exit(-1);
	}
	if ((sofile[dc.flevel + 1] = fopen(name, "r")) == NULL_FPTR) {
		fprintf(err_stream,
			"***%s: unable to open %s\n", myname, name);
		err_exit(-1);
	}
	dc.flevel += 1;
	break;

      case SP:
	/* Space:  .sp [N=1] */
	set(&spval, val, argtyp, 1, 0, HUGE);
	space(spval);
	break;

      case SS:
	/* Space char size (IGNORED):  .ss N */
	break;

      case TI:
	/* Temporary indent:  .ti [+/-N] */
	robrk();
	set(&dc.tival, val, argtyp, 0, 0, dc.rmval);
	break;

      case UL:
	/* Underline:  .ul [N] */
	set(&dc.ulval, val, argtyp, 0, 1, HUGE);
	dc.cuval = dc.boval = 0;
	break;

  }
}


/*------------------------------*/
/*	comtyp			*/
/*------------------------------*/
int comtyp(p, m)
register char *p;
char *m;
{

/* Decodes nro command and returns its associated value.
 * ptr "p" is incremented (and returned)
 */

  register char c1;
  register char c2;
  char *s;
  char macnam[MNLEN];

  /* Skip past dot and any whitespace */
  p++;
  while (*p && (*p == ' ' || *p == '\t')) p++;
  if (*p == '\0') return (COMMENT);

  /* First check to see if the command is a macro. If it is, truncate
   * to two characters and return expansion in m.  Note that upper and
   * lower case characters are handled differently. 
   */
  getwrd(p, macnam);
  macnam[2] = EOS;
  if ((s = getmac(macnam)) != NULL_CPTR) {
	strcpy(m, s);
	return(MACRO);
  }
  c1 = *p++;
  c2 = *p;
  if (c1 == '\\' && c2 == '\"') return(COMMENT);
  if (c1 == 'a' && c2 == 'f') return (AF);
  if (c1 == 'a' && c2 == 'd') return (AD);
  if (c1 == 'b' && c2 == 'o') return (BO);
  if (c1 == 'b' && c2 == 'p') return (BP);
  if (c1 == 'b' && c2 == 'r') return (BR);
  if (c1 == 'b' && c2 == 's') return (BS);
  if (c1 == 'c' && c2 == 'c') return (CC);
  if (c1 == 'c' && c2 == 'e') return (CE);
  if (c1 == 'c' && c2 == 'u') return (CU);
  if (c1 == 'd' && c2 == 'e') return (DE);
  if (c1 == 'd' && c2 == 's') return (DS);
  if (c1 == 'e' && c2 == 'f') return (EF);
  if (c1 == 'e' && c2 == 'c') return (EC);
  if (c1 == 'e' && c2 == 'h') return (EH);
  if (c1 == 'e' && c2 == 'n') return (EN);
  if (c1 == '.') return (EN);
  if (c1 == 'e' && c2 == 'o') return (EO);
  if (c1 == 'f' && c2 == 'i') return (FI);
  if (c1 == 'f' && c2 == 'l') return (FL);
  if (c1 == 'f' && c2 == 'o') return (FO);
  if (c1 == 'f' && c2 == 't') return (FT);
  if (c1 == 'h' && c2 == 'e') return (HE);
  if (c1 == 'i' && c2 == 'n') return (IN);
  if (c1 == 'j' && c2 == 'u') return (JU);
  if (c1 == 'l' && c2 == 'l') return (LL);
  if (c1 == 'l' && c2 == 's') return (LS);
  if (c1 == 'm' && c2 == '1') return (M1);
  if (c1 == 'm' && c2 == '2') return (M2);
  if (c1 == 'm' && c2 == '3') return (M3);
  if (c1 == 'm' && c2 == '4') return (M4);
  if (c1 == 'n' && c2 == 'a') return (NA);
  if (c1 == 'n' && c2 == 'e') return (NE);
  if (c1 == 'n' && c2 == 'f') return (NF);
  if (c1 == 'n' && c2 == 'j') return (NJ);
  if (c1 == 'n' && c2 == 'r') return (NR);
  if (c1 == 'o' && c2 == 'f') return (OF);
  if (c1 == 'o' && c2 == 'h') return (OH);
  if (c1 == 'p' && c2 == 'c') return (PC);
  if (c1 == 'p' && c2 == 'l') return (PL);
  if (c1 == 'p' && c2 == 'm') return (PM);
  if (c1 == 'p' && c2 == 'o') return (PO);
  if (c1 == 'r' && c2 == 'm') return (RM);
  if (c1 == 'r' && c2 == 'r') return (RR);
  if (c1 == 's' && c2 == 'o') return (SO);
  if (c1 == 's' && c2 == 'p') return (SP);
  if (c1 == 't' && c2 == 'i') return (TI);
  if (c1 == 't' && c2 == 'l') return (TL);
  if (c1 == 'u' && c2 == 'l') return (UL);

  if (c1 == 'p' && c2 == 'n') return(PN);
  if (c1 == 'r' && c2 == 'r') return (RR);
  if (c1 == 'c' && c2 == '2') return (C2);
  if (c1 == 't' && c2 == 'r') return (TR);
  if (c1 == 'l' && c2 == 't') return (LT);
  if (c1 == 'f' && c2 == 'c') return (FC);

  return(UNKNOWN);
}


/*------------------------------*/
/*	gettl			*/
/*------------------------------*/
void gettl(p, q, limit)
register char *p;
register char *q;
int *limit;
{

/* Get header or footer title  */

  /* Skip forward a word... */
  p = skipwd(p);
  p = skipbl(p);

  /* Copy and set limits */
  strcpy(q, p);
  limit[LEFT] = dc.inval;
  limit[RIGHT] = dc.rmval;
}


/*------------------------------*/
/*	getval			*/
/*------------------------------*/
int getval(p, p_argt)
register char *p;
register char *p_argt;
{

/* Retrieves optional argument following command.
 * Returns positive integer value with sign (if any)
 * saved in character addressed by p_argt.
 */

  p = skipwd(p);
  p = skipbl(p);
  *p_argt = *p;
  if ((*p == '+') || (*p == '-')) ++p;
  return(ctod(p));
}


/*------------------------------*/
/*	set			*/
/*------------------------------*/
void set(param, val, type, defval, minval, maxval)
register int *param;
register int val;
register char type;
register int defval;
register int minval;
register int maxval;
{

/*
 *	set parameter and check range. this is for basically all commands
 *	which take interger args
 *
 *	no param (i.e. \r or \n) means reset default
 *	+ means param += val (increment)
 *	- means param -= val (decrement)
 *	anything else makes an assignment within the defined numerical limits
 *
 *	examples:
 *
 *	.nr a 14	set register 'a' to 14
 *	.nr a +1	increment register 'a' by 1
 *	.nr a		reset register 'a' to default value (0)
 */

  switch (type) {
      case '\r':
      case '\n':	*param = defval;	break;
      case '+':	*param += val;			break;
      case '-':	*param -= val;			break;
      default:	*param = val;			break;

  }
  *param = min(*param, maxval);
  *param = max(*param, minval);
}

/*------------------------------*/
/*	set_ireg		*/
/*------------------------------*/
int set_ireg(name, val, opt)
register char *name;
register int val;
register int opt;		/* 0=internal, 1=user set */
{

/* Set internal register "name" to val. ret 0 if ok, else -1 if reg not
 * found or 1 if read only
 */

  register int nreg;

  nreg = findreg(name);
  if (nreg < 0) return(-1);

  if ((rg[nreg].rflag & RF_WRITE) || (opt == 0)) {
	rg[nreg].rval = val;

	return(0);
  }
  return(1);
}
