/* main.c - main for nroff word processor */

#define NRO_MAIN

#include <stdio.h>
#include "config.h"
#include "nroff.h"

int main(argc, argv)
int argc;
char *argv[];
{
  register int i;
  int swflg;
  int ifp = 0;
  char *ptmp;
  char *pterm;
  char capability[100];
  char *pcap;
  char *ps;

  /* Set up initial flags and file descriptors */
  swflg = FALSE;
  ignoring = FALSE;
  hold_screen = FALSE;
  debugging = FALSE;
  stepping = FALSE;
  out_stream = stdout;
  err_stream = stderr;
  dbg_stream = stderr;

  /* This is for tmp files, if ever needed. it SHOULD start out without
   * the trailing slash. if not in env, use default 
   */
  if (ptmp = getenv("TMPDIR"))
	strcpy(tmpdir, ptmp);
  else
	strcpy(tmpdir, ".");

  /* Handle terminal for \fB, \fI */
  s_italic[0] = '\0';
  e_italic[0] = '\0';
  s_bold[0] = '\0';
  e_bold[0] = '\0';
  if ((pterm = getenv("TERM")) && (tgetent(termcap, pterm) == 1)) {
	/* Termcap capabilities md/me for bold, us/ue for italic,
	 * so/se if all else fails */
	pcap = capability;
	if (ps = tgetstr("so", &pcap)) {

/* NOTE: the termcap on my sun has padding or something in it so i just
   arbitarily remove it here. this is not right, but the worst that happens
   is you have no standout. since minix uses standard ansi escape for so,
   \E[7m, this should not be a problem. also i rarely use any non-ansi
   terminals so this is also not a problem for me. the right thing to do
   would be to use tputs() to remove the padding and write a new string
   but i am lazy... */
		while (*ps && *ps != 0x1B) ps++;
		strcpy(s_italic, ps);
		strcpy(s_bold, ps);
	}
	if (ps = tgetstr("se", &pcap)) {
		while (*ps && *ps != 0x1B) ps++;
		strcpy(e_italic, ps);
		strcpy(e_bold, ps);
	}

	/* Because the type faces are actually exclusive and the
	 * terminal capabilities are not the one has turn the other
	 * off before starting itself */
	/* End Italic */
	if (ps = tgetstr("ue", &pcap)) {
		while (*ps && *ps != 0x1B) ps++;
		strcpy(e_italic, ps);
	}

	/* End Bold */
	if (ps = tgetstr("me", &pcap)) {
		while (*ps && *ps != 0x1B) ps++;
		strcpy(e_bold, ps);
	}

	/* Start Italic */
	if (ps = tgetstr("us", &pcap)) {
		while (*ps && *ps != 0x1B) ps++;
		strcpy(s_italic, e_bold);
		strcat(s_italic, ps);
	}

	/* Start Bold */
	if (ps = tgetstr("md", &pcap)) {
		while (*ps && *ps != 0x1B) ps++;
		strcpy(s_bold, e_italic);
		strcat(s_bold, ps);
	}
  }

  /* Initialize structures (defaults) */
  init();

  /* Parse cmdline flags */
  for (i = 1; i < argc; ++i) {
	if (*argv[i] == '-' || *argv[i] == '+') {
		if (pswitch(argv[i], argv[i + 1], &swflg) == ERR)
			err_exit(-1);
	}
  }

  /* Loop on files */
  for (i = 1; i < argc; ++i) {
	if (*argv[i] != '-' && *argv[i] != '+') {
		/* Open this file... */
		if ((sofile[0] = fopen(argv[i], "r")) == NULL_FPTR) {
			fprintf(err_stream,
				"***%s: unable to open file %s\n",
				myname, argv[i]);
			err_exit(-1);
		} else {
			/* Do it for this file... */
			ifp = 1;
			profile();
			fclose(sofile[0]);
		}
	} else if (*argv[i] == '-' && *(argv[i] + 1) == 0) {
		/* - means read stdin (anywhere in file list) */
		sofile[0] = stdin;
		ifp = 1;
		profile();
	}
  }

  /* If no files, usage (should really use stdin...) */
  if ((ifp == 0 && swflg == FALSE) || argc <= 1) {
	usage();

	err_exit(-1);
  }

  /* Normal exit. this will fflush/fclose streams... */
  err_exit(0);
  exit(-1);
}

/*------------------------------*/
/*	usage			*/
/*------------------------------*/
void usage()
{
  /* Note: -l may not work correctly */
  fprintf(stderr, "Usage:   %s [options] file [...]\n", myname);
  fprintf(stderr, "Options: -a        no font changes\n");
  fprintf(stderr, "         -b        backspace\n");
  fprintf(stderr, "         -d        debug mode (file: nroff.dbg)\n");
  fprintf(stderr, "         -l        output to printer\n");
  fprintf(stderr, "         -m<name>  macro file (e.g. -man)\n");
  fprintf(stderr, "         -o file   error log file (stderr is default)\n");
  fprintf(stderr, "         -pl<n>    page length\n");
  fprintf(stderr, "         -po<n>    page offset\n");
  fprintf(stderr, "         -pn<n>    initial page number\n");
  fprintf(stderr, "         -s        step through pages\n");
  fprintf(stderr, "         -v        print version only\n");
  fprintf(stderr, "         +<n>      first page to do\n");
  fprintf(stderr, "         -<n>      last page to do\n");
  fprintf(stderr, "         -         use stdin (in file list)\n");
}




/*------------------------------*/
/*	init			*/
/*------------------------------*/
void init()
{

/*
 *	initialize parameters for nro word processor
 */


  register int i;
  time_t tval;
  char *ctim;

  tval = time((time_t *) 0);
  dc.fill = YES;
  dc.dofnt = YES;
  dc.lsval = 1;
  dc.inval = 0;
  dc.rmval = PAGEWIDTH - 1;
  dc.llval = PAGEWIDTH - 1;
  dc.ltval = PAGEWIDTH - 1;
  dc.tival = 0;
  dc.ceval = 0;
  dc.ulval = 0;
  dc.cuval = 0;
  dc.juval = YES;
  dc.adjval = ADJ_BOTH;
  dc.boval = 0;
  dc.bsflg = FALSE;
  dc.prflg = TRUE;
  dc.sprdir = 0;
  dc.flevel = 0;
  dc.lastfnt = 1;
  dc.thisfnt = 1;
  dc.escon = YES;
  dc.pgchr = '%';
  dc.cmdchr = '.';
  dc.escchr = '\\';
  dc.nobrchr = '\'';
  for (i = 0; i < 26; ++i) dc.nr[i] = 0;
  for (i = 0; i < 26; ++i) dc.nrauto[i] = 1;
  for (i = 0; i < 26; ++i) dc.nrfmt[i] = '1';


  /* Initialize internal regs. first zero out... */
  for (i = 0; i < MAXREGS; i++) {
	rg[i].rname[0] = rg[i].rname[1] = rg[i].rname[2] = rg[i].rname[3] = '\0';
	rg[i].rauto = 1;
	rg[i].rval = 0;
	rg[i].rflag = RF_READ;
	rg[i].rfmt = '1';
  }



  /* This should be checked... */
  ctim = ctime(&tval);


  /* Predefined regs. these are read/write: */
  i = 0;

  strcpy(rg[i].rname, "%");	/* current page */
  rg[i].rauto = 1;
  rg[i].rval = 0;
  rg[i].rflag = RF_READ | RF_WRITE;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, "ct");	/* character type */
  rg[i].rauto = 1;
  rg[i].rval = 0;
  rg[i].rflag = RF_READ | RF_WRITE;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, "dl");	/* width of last complete di */
  rg[i].rauto = 1;
  rg[i].rval = 0;
  rg[i].rflag = RF_READ | RF_WRITE;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, "dn");	/* height of last complete di */
  rg[i].rauto = 1;
  rg[i].rval = 0;
  rg[i].rflag = RF_READ | RF_WRITE;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, "dw");	/* day of week (1-7) */
  rg[i].rval = 0;
  if (!strncmp(&ctim[0], "Sun", 3))
	rg[i].rval = 1;
  else if (!strncmp(&ctim[0], "Mon", 3))
	rg[i].rval = 2;
  else if (!strncmp(&ctim[0], "Tue", 3))
	rg[i].rval = 3;
  else if (!strncmp(&ctim[0], "Wed", 3))
	rg[i].rval = 4;
  else if (!strncmp(&ctim[0], "Thu", 3))
	rg[i].rval = 5;
  else if (!strncmp(&ctim[0], "Fri", 3))
	rg[i].rval = 6;
  else if (!strncmp(&ctim[0], "Sat", 3))
	rg[i].rval = 7;
  rg[i].rauto = 1;
  rg[i].rflag = RF_READ | RF_WRITE;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, "dy");	/* day of month (1-31) */
  rg[i].rauto = 1;
  rg[i].rval = atoi(&ctim[8]);
  rg[i].rflag = RF_READ | RF_WRITE;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, "hp");	/* current h pos on input */
  rg[i].rauto = 1;
  rg[i].rval = 0;
  rg[i].rflag = RF_READ | RF_WRITE;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, "ln");	/* output line num */
  rg[i].rauto = 1;
  rg[i].rval = 0;
  rg[i].rflag = RF_READ | RF_WRITE;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, "mo");	/* current month (1-12) */
  rg[i].rval = 0;
  if (!strncmp(&ctim[4], "Jan", 3))
	rg[i].rval = 1;
  else if (!strncmp(&ctim[4], "Feb", 3))
	rg[i].rval = 2;
  else if (!strncmp(&ctim[4], "Mar", 3))
	rg[i].rval = 3;
  else if (!strncmp(&ctim[4], "Apr", 3))
	rg[i].rval = 4;
  else if (!strncmp(&ctim[4], "May", 3))
	rg[i].rval = 5;
  else if (!strncmp(&ctim[4], "Jun", 3))
	rg[i].rval = 6;
  else if (!strncmp(&ctim[4], "Jul", 3))
	rg[i].rval = 7;
  else if (!strncmp(&ctim[4], "Aug", 3))
	rg[i].rval = 8;
  else if (!strncmp(&ctim[4], "Sep", 3))
	rg[i].rval = 9;
  else if (!strncmp(&ctim[4], "Oct", 3))
	rg[i].rval = 10;
  else if (!strncmp(&ctim[4], "Nov", 3))
	rg[i].rval = 11;
  else if (!strncmp(&ctim[4], "Dec", 3))
	rg[i].rval = 12;
  rg[i].rauto = 1;
  rg[i].rflag = RF_READ | RF_WRITE;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, "nl");	/* v pos of last base-line */
  rg[i].rauto = 1;
  rg[i].rval = 0;
  rg[i].rflag = RF_READ | RF_WRITE;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, "sb");	/* depth of str below base */
  rg[i].rauto = 1;
  rg[i].rval = 0;
  rg[i].rflag = RF_READ | RF_WRITE;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, "st");	/* height of str above base */
  rg[i].rauto = 1;
  rg[i].rval = 0;
  rg[i].rflag = RF_READ | RF_WRITE;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, "yr");	/* last 2 dig of current year */
  rg[i].rauto = 1;
  rg[i].rval = atoi(&ctim[22]);
  rg[i].rflag = RF_READ | RF_WRITE;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, "hh");	/* current hour (0-23) */
  rg[i].rauto = 1;
  rg[i].rval = atoi(&ctim[11]);
  rg[i].rflag = RF_READ | RF_WRITE;
  rg[i].rfmt = 2 | 0x80;
  i++;

  strcpy(rg[i].rname, "mm");	/* current minute (0-59) */
  rg[i].rauto = 1;
  rg[i].rval = atoi(&ctim[14]);
  rg[i].rflag = RF_READ | RF_WRITE;
  rg[i].rfmt = 2 | 0x80;
  i++;

  strcpy(rg[i].rname, "ss");	/* current second (0-59) */
  rg[i].rauto = 1;
  rg[i].rval = atoi(&ctim[17]);
  rg[i].rflag = RF_READ | RF_WRITE;
  rg[i].rfmt = 2 | 0x80;
  i++;


  /* These are read only: */
  strcpy(rg[i].rname, ".$");	/* num of args at current macro */
  rg[i].rauto = 1;
  rg[i].rval = 0;
  rg[i].rflag = RF_READ;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, ".A");	/* 1 for nroff */
  rg[i].rauto = 1;
  rg[i].rval = 1;
  rg[i].rflag = RF_READ;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, ".H");	/* hor resolution */
  rg[i].rauto = 1;
  rg[i].rval = 1;
  rg[i].rflag = RF_READ;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, ".T");	/* 1 for troff */
  rg[i].rauto = 0;
  rg[i].rval = 0;
  rg[i].rflag = RF_READ;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, ".V");	/* vert resolution */
  rg[i].rauto = 1;
  rg[i].rval = 1;
  rg[i].rflag = RF_READ;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, ".a");
  rg[i].rauto = 1;
  rg[i].rval = 0;
  rg[i].rflag = RF_READ;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, ".c");
  rg[i].rauto = 1;
  rg[i].rval = 0;
  rg[i].rflag = RF_READ;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, ".d");
  rg[i].rauto = 1;
  rg[i].rval = 0;
  rg[i].rflag = RF_READ;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, ".f");	/* current font (1-4) */
  rg[i].rauto = 1;
  rg[i].rval = 1;
  rg[i].rflag = RF_READ;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, ".h");
  rg[i].rauto = 1;
  rg[i].rval = 0;
  rg[i].rflag = RF_READ;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, ".i");	/* current indent */
  rg[i].rauto = 1;
  rg[i].rval = 0;
  rg[i].rflag = RF_READ;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, ".l");	/* current line length */
  rg[i].rauto = 1;
  rg[i].rval = PAGEWIDTH - 1;
  rg[i].rflag = RF_READ;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, ".n");
  rg[i].rauto = 1;
  rg[i].rval = 0;
  rg[i].rflag = RF_READ;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, ".o");	/* current offset */
  rg[i].rauto = 1;
  rg[i].rval = 0;
  rg[i].rflag = RF_READ;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, ".p");	/* current page len */
  rg[i].rauto = 1;
  rg[i].rval = PAGELEN;
  rg[i].rflag = RF_READ;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, ".s");	/* current point size */
  rg[i].rauto = 1;
  rg[i].rval = 1;
  rg[i].rflag = RF_READ;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, ".t");
  rg[i].rauto = 1;
  rg[i].rval = 0;
  rg[i].rflag = RF_READ;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, ".u");
  rg[i].rauto = 1;
  rg[i].rval = 0;
  rg[i].rflag = RF_READ;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, ".v");	/* current v line spacing */
  rg[i].rauto = 1;
  rg[i].rval = 1;
  rg[i].rflag = RF_READ;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, ".w");	/* width of prev char */
  rg[i].rauto = 1;
  rg[i].rval = 1;
  rg[i].rflag = RF_READ;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, ".x");
  rg[i].rauto = 1;
  rg[i].rval = 0;
  rg[i].rflag = RF_READ;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, ".y");
  rg[i].rauto = 1;
  rg[i].rval = 0;
  rg[i].rflag = RF_READ;
  rg[i].rfmt = '1';
  i++;

  strcpy(rg[i].rname, ".z");
  rg[i].rauto = 1;
  rg[i].rval = 0;
  rg[i].rflag = RF_READ;
  rg[i].rfmt = '1';

  pg.curpag = 0;
  pg.newpag = 1;
  pg.lineno = 0;
  pg.plval = PAGELEN;
  pg.m1val = 2;
  pg.m2val = 2;
  pg.m3val = 2;
  pg.m4val = 2;
  pg.bottom = pg.plval - pg.m4val - pg.m3val;
  pg.offset = 0;
  pg.frstpg = 0;
  pg.lastpg = 30000;
  pg.ehead[0] = pg.ohead[0] = '\n';
  pg.efoot[0] = pg.ofoot[0] = '\n';
  for (i = 1; i < MAXLINE; ++i) {
	pg.ehead[i] = pg.ohead[i] = EOS;
	pg.efoot[i] = pg.ofoot[i] = EOS;
  }
  pg.ehlim[LEFT] = pg.ohlim[LEFT] = dc.inval;
  pg.eflim[LEFT] = pg.oflim[LEFT] = dc.inval;
  pg.ehlim[RIGHT] = pg.ohlim[RIGHT] = dc.rmval;
  pg.eflim[RIGHT] = pg.oflim[RIGHT] = dc.rmval;

  co.outp = 0;
  co.outw = 0;
  co.outwds = 0;
  co.lpr = FALSE;
  for (i = 0; i < MAXLINE; ++i) co.outbuf[i] = EOS;

  for (i = 0; i < MXMDEF; ++i) mac.mnames[i] = NULL_CPTR;
  for (i = 0; i < MACBUF; ++i) mac.mb[i] = '\0';
  for (i = 0; i < MAXLINE; ++i) mac.pbb[i] = '\0';
  mac.lastp = 0;
  mac.emb = &mac.mb[0];
  mac.ppb = NULL_CPTR;
}




/*------------------------------*/
/*	pswitch			*/
/*------------------------------*/
int pswitch(p, p2, q)
register char *p;
register char *p2;
register int *q;
{

/*
 *	process switch values from command line
 */

  int swgood;
  char mfile[128];
  char *ptmac;

  swgood = TRUE;
  if (*p == '-') {
	/* Since is STILL use the goofy atari/dri xmain code, i look
	 * for both upper and lower case. if you use dLibs (and if
	 * its startup code does not ucase the cmd line), you can
	 * probably look for just lower case. gulam and other shells
	 * typically don't change case of cmd line. 
	 */
	switch (*++p) {
	    case 0:		/* stdin */
		break;

	    case 'a':		/* font changes */
	    case 'A':	dc.dofnt = NO;	break;

	    case 'b':		/* backspace */
	    case 'B':	dc.bsflg = TRUE;	break;

	    case 'd':		/* debug mode */
	    case 'D':
		dbg_stream = fopen(dbgfile, "w");
		debugging = TRUE;
		break;

	    case 'h':		/* hold screen */
	    case 'H':	hold_screen = TRUE;	break;

	    case 'l':		/* to lpr (was P) */
	    case 'L':
		out_stream = fopen(printer, "w");
		co.lpr = TRUE;
		break;

	    case 'm':		/* macro file */
	    case 'M':
		/* Build macro file name. start with lib 
		 * put c:\lib\tmac in environment so we can read it
		 * here. else use default. if you want file from cwd,
		 * "setenv TMACDIR ." from shell.
		 * 
		 * we want file names like "tmac.an" (for -man) 
		 */
		if (ptmac = getenv("TMACDIR")) {
			/* This is the lib path (e.g. "c:\lib\tmac") */
			strcpy(mfile, ptmac);

			/* This is the prefix (i.e. "\tmac.") */
			strcat(mfile, TMACPRE);
		} else
			/* Use default lib/prefix (i.e.
			 * "c:\lib\tmac\tmac.") */
			strcpy(mfile, TMACFULL);

		/* Finally, add extension (e.g. "an") */
		strcat(mfile, ++p);

		/* Open file and read it */
		if ((sofile[0] = fopen(mfile, "r")) == NULL_FPTR) {
			fprintf(stderr,
			    "***%s: unable to open macro file %s\n",
				myname, mfile);
			err_exit(-1);
		}
		profile();
		fclose(sofile[0]);
		break;

	    case 'o':		/* output error log */
	    case 'O':
		if (!p2) {
			fprintf(stderr,
				"***%s: no error file specified\n",
				myname);
			err_exit(-1);
		}
		if ((err_stream = fopen(p2, "w")) == NULL_FPTR) {
			fprintf(stderr,
			    "***%s: unable to open error file %s\n",
				myname, p2);
			err_exit(-1);
		}
		break;

	    case 'p':		/* .po, .pn */
	    case 'P':
		if (*(p + 1) == 'o' || *(p + 1) == 'O') {	/* -po___ */
			p += 2;
			set(&pg.offset, ctod(p), '1', 0, 0, HUGE);
			set_ireg(".o", pg.offset, 0);
		} else if (*(p + 1) == 'n' || *(p + 1) == 'N') {	/* -pn___ */
			p += 2;
			set(&pg.curpag, ctod(p) - 1, '1', 0, -HUGE, HUGE);
			pg.newpag = pg.curpag + 1;
			set_ireg("%", pg.newpag, 0);
		} else if (*(p + 1) == 'l' || *(p + 1) == 'L') {	/* -pl___ */
			p += 2;
			set(&pg.plval, ctod(p) - 1, '1', 0,
			    pg.m1val + pg.m2val + pg.m3val + pg.m4val + 1,
			    HUGE);
			set_ireg(".p", pg.plval, 0);
			pg.bottom = pg.plval - pg.m3val - pg.m4val;
		} else {	/* -p___ */
			set(&pg.offset, ctod(++p), '1', 0, 0, HUGE);
			set_ireg(".o", pg.offset, 0);
		}
		break;

	    case 's':		/* page step mode */
	    case 'S':	stepping = TRUE;	break;

	    case 'v':		/* version */
	    case 'V':
		printf("%s\n", "Version 1.5");
		*q = TRUE;
		break;

	    case '0':		/* last page */
	    case '1':
	    case '2':
	    case '3':
	    case '4':
	    case '5':
	    case '6':
	    case '7':
	    case '8':
	    case '9':	pg.lastpg = ctod(p);	break;

	    default:		/* illegal */
		swgood = FALSE;
		break;
	}
  } else if (*p == '+') {	/* first page */
	pg.frstpg = ctod(++p);
  } else {			/* illegal */
	swgood = FALSE;
  }


  if (swgood == FALSE) {
	fprintf(stderr, "***%s: illegal switch %s\n", myname, p);
	return(ERR);
  }
  return(OK);
}




/*------------------------------*/
/*	profile			*/
/*------------------------------*/
void profile()
{

/*
 *	process input files from command line
 */

  char ibuf[MAXLINE];

  /* Handle nesting of includes (.so) */
  for (dc.flevel = 0; dc.flevel >= 0; dc.flevel -= 1) {
	while (getlin(ibuf, sofile[dc.flevel]) != EOF) {
		/* If line is a command or text */
		if (ibuf[0] == dc.cmdchr) {
			comand(ibuf);
		} else {
			/* This is a text line. first see if first
			 * char is space. if it is, break line. */
			if (ibuf[0] == ' ') robrk();
			text(ibuf);
		}
	}

	/* Close included file */
	if (dc.flevel > 0) fclose(sofile[dc.flevel]);
  }
  if (pg.lineno > 0) space(HUGE);
}
