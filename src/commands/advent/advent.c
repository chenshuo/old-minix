/**	Adventure ported to Minix by:
  Robert R. Hall (hall@nosc.mil)
  Naval Ocean Center
  San Diego,  Calif
 */

/**	program ADVENT.C					*
 *	WARNING: "advent.c" allocates GLOBAL storage space by	*
 *		including "advdef.h".				*
 *		All other modules use "advdec.h"		*/


#include	<sys/types.h>
#include        <string.h>
#include	<ctype.h>
#include	<unistd.h>	/* drv = 1.1st file 2.def 3.A	 */
#include	<stdio.h>	/* drv = 1.1st file 2.def 3.A	 */
#include	"advent.h"	/* #define preprocessor equates	 */
#include	"advword.h"	/* definition of "word" array	 */
#include	"advtext.h"	/* definition of "text" arrays	 */
#include	"advdef.h"

_PROTOTYPE (void exit, (int status));

char *textdir = "/usr/src/data";/* directory where text files live. */

int main(argc, argv)
int argc;
char **argv;
{
  int n, rflag;			/* user restore request option	 */

  n = chdir(textdir);		/* all the goodies are kept there. */
  if (n < 0) {
	printf("Unable to chdir(%s) where text files must be kept\n", textdir);
	exit(1);
  }
  rflag = 0;
  g.dbugflg = 0;
  while (--argc > 0) {
	++argv;
	if (**argv != '-') break;
	switch (tolower(argv[0][1])) {
	    case 'r':
		++rflag;
		continue;
	    case 'd':
		++g.dbugflg;
		continue;
	    default:
		printf("unknown flag: %c\n", argv[0][1]);
		continue;
	}			/* switch	 */
  }				/* while	 */
  if (g.dbugflg < 2) g.dbugflg = 0;	/* must request three times	 */
  opentxt();
  initplay();
  if (rflag)
	restore();
  else if (yes(65, 1, 0))
	g.limit = 1000;
  else
	g.limit = 330;
  g.saveflg = 0;
  srand(511);			/* seed random	 */
  while (!g.saveflg) turn();
  if (g.saveflg) saveadv();
  fclose(fd1);
  fclose(fd2);
  fclose(fd3);
  fclose(fd4);
  exit(0);			/* exit = ok	 */
}				/* main		 */

/* ************************************************************	*/

/*
  Initialize integer arrays with sscanf
*/
void scanint(pi, str)
int *pi;
char *str;
{

  while (*str) {
	if ((sscanf(str, "%d,", pi++)) < 1)
		bug(41);	/* failed before EOS		 */
	while (*str++ != ',')	/* advance str pointer	 */
		;
  }
  return;
}

/*
  Initialization of adventure play variables
*/
void initplay()
{
  g.turns = 0;

  /* Initialize location status array */
  memset(g.cond, 0, (sizeof(int)) * MAXLOC);
  scanint(&g.cond[1], "5,1,5,5,1,1,5,17,1,1,");
  scanint(&g.cond[13], "32,0,0,2,0,0,64,2,");
  scanint(&g.cond[21], "2,2,0,6,0,2,");
  scanint(&g.cond[31], "2,2,0,0,0,0,0,4,0,2,");
  scanint(&g.cond[42], "128,128,128,128,136,136,136,128,128,");
  scanint(&g.cond[51], "128,128,136,128,136,0,8,0,2,");
  scanint(&g.cond[79], "2,128,128,136,0,0,8,136,128,0,2,2,");
  scanint(&g.cond[95], "4,0,0,0,0,1,");
  scanint(&g.cond[113], "4,0,1,1,");
  scanint(&g.cond[122], "8,8,8,8,8,8,8,8,8,");

  /* Initialize object locations */
  memset(g.place, 0, (sizeof(int)) * MAXOBJ);
  scanint(&g.place[1], "3,3,8,10,11,115,14,13,94,96,");
  scanint(&g.place[11], "19,17,101,103,0,106,0,0,3,3,");
  scanint(&g.place[23], "109,25,23,111,35,0,97,");
  scanint(&g.place[31], "119,117,117,0,130,0,126,140,0,96,");
  scanint(&g.place[50], "18,27,28,29,30,");
  scanint(&g.place[56], "92,95,97,100,101,0,119,127,130,");

  /* Initialize second (fixed) locations */
  memset(g.fixed, 0, (sizeof(int)) * MAXOBJ);
  scanint(&g.fixed[3], "9,0,0,0,15,0,-1,");
  scanint(&g.fixed[11], "-1,27,-1,0,0,0,-1,");
  scanint(&g.fixed[23], "-1,-1,67,-1,110,0,-1,-1,");
  scanint(&g.fixed[31], "121,122,122,0,-1,-1,-1,-1,0,-1,");
  scanint(&g.fixed[62], "121,");

  /* Initialize default verb messages */
  scanint(actmsg, "0,24,29,0,33,0,33,38,38,42,14,");
  scanint(&actmsg[11], "43,110,29,110,73,75,29,13,59,59,");
  scanint(&actmsg[21], "174,109,67,13,147,155,195,146,110,13,13,");

  /* Initialize various flags and other variables */
  memset(g.visited, 0, (sizeof(int)) * MAXLOC);
  memset(g.prop, 0, (sizeof(int)) * MAXOBJ);
  memset(&g.prop[50], 0xff, (sizeof(int)) * (MAXOBJ - 50));
  g.wzdark = g.closed = g.closing = g.holding = g.detail = 0;
  g.limit = 100;
  g.tally = 15;
  g.tally2 = 0;
  g.newloc = 1;
  g.loc = g.oldloc = g.oldloc2 = 3;
  g.knfloc = 0;
  g.chloc = 114;
  g.chloc2 = 140;
/*	g.dloc[DWARFMAX-1] = chloc				*/
  scanint(g.dloc, "0,19,27,33,44,64,114,");
  scanint(g.odloc, "0,0,0,0,0,0,0,");
  g.dkill = 0;
  scanint(g.dseen, "0,0,0,0,0,0,0,");
  g.clock = 30;
  g.clock2 = 50;
  g.panic = 0;
  g.bonus = 0;
  g.numdie = 0;
  g.daltloc = 18;
  g.lmwarn = 0;
  g.foobar = 0;
  g.dflag = 0;
  g.gaveup = 0;
  g.saveflg = 0;
  return;
}

/*
  Open advent?.txt files
*/
void opentxt()
{
  fd1 = fopen("advent1.txt", "r");
  if (!fd1) {
	printf("Sorry, I can't open advent1.txt...\n");
	exit(1);
  }
  fd2 = fopen("advent2.txt", "r");
  if (!fd2) {
	printf("Sorry, I can't open advent2.txt...\n");
	exit(1);
  }
  fd3 = fopen("advent3.txt", "r");
  if (!fd3) {
	printf("Sorry, I can't open advent3.txt...\n");
	exit(1);
  }
  fd4 = fopen("advent4.txt", "r");
  if (!fd4) {
	printf("Sorry, I can't open advent4.txt...\n");
	exit(1);
  }
}

/*
	save adventure game
*/
void saveadv()
{
  char *sptr;
  FILE *savefd;
  char username[13];

  printf("What do you want to name the saved game? \n");
  scanf("%s", username);
  if (sptr = strchr(username, '.'))
	*sptr = '\0';		/* kill extension	 */
  if (strlen(username) > 8)
	username[8] = '\0';	/* max 8 char filename	 */
  strcat(username, ".adv");
  savefd = fopen(username, "wb");
  if (savefd == NULL) {
	printf("Sorry, I can't create the file...%s\n",
	       username);
	exit(1);
  }
  for (sptr = (char *) &g.turns; sptr < (char *) &g.lastglob; sptr++) {
	if (fputc(*sptr, savefd) == EOF) {
		printf("Write error on save file...%s\n",
		       username);
		exit(1);
	}
  }
  if (fclose(savefd) == -1) {
	printf("Sorry, I can't close the file...%s\n",
	       username);
	exit(1);
  }
  printf("OK -- \"C\" you later...\n");
}

/*
  restore saved game handler
*/
void restore()
{
  char username[13];
  int c;
  FILE *restfd;
  char *sptr;

  printf("What is the name of the saved game? \n");
  scanf("%s", username);
  if (sptr = strchr(username, '.'))
	*sptr = '\0';		/* kill extension	 */
  if (strlen(username) > 8)
	username[8] = '\0';	/* max 8 char filename	 */
  strcat(username, ".adv");
  restfd = fopen(username, "rb");
  if (restfd == NULL) {
	printf("Sorry, I can't open the file...%s\n",
	       username);
	exit(1);
  }
  for (sptr = (char *) &g.turns; sptr < (char *) &g.lastglob; sptr++) {
	if ((c = fgetc(restfd)) == -1) {
		printf("Read error on save file...%s\n",
		       username);
		exit(1);
	}
	*sptr = c;
  }
  if (fclose(restfd) == -1) {
	printf("Warning -- can't close save file...%s\n",
	       username);
  }
}
