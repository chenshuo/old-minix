
/**	program DATABASE.C					*
 *	WARNING: "advent.c" allocates GLOBAL storage space by	*
 *		including "advdef.h".				*
 *		All other modules use "advdec.h".		*/


#include        <string.h>
#include	<ctype.h>
#include	<stdio.h>
#include	"advent.h"
#include	"advdec.h"
#include	"advcave.h"

_PROTOTYPE (void exit, (int status));

/*
  Routine to fill travel array for a given location
*/
void gettrav(loc)
int loc;
{
  int i;
  long t, *lptr;

  lptr = cave[loc - 1];
  for (i = 0; i < MAXTRAV; i++) {
	t = *lptr++;
	if (!(t)) {
		travel[i].tdest = -1;	/* end of array	 */
		if (g.dbugflg) for (i = 0; i < MAXTRAV; ++i)
				printf("cave[%d] = %d %d %d\n",
				       loc, travel[i].tdest,
				  travel[i].tverb, travel[i].tcond);
		return;		/* terminate for loop	 */
	}
	travel[i].tcond = (t % 1000);
	t /= 1000;
	travel[i].tverb = (t % 1000);
	t /= 1000;
	travel[i].tdest = (t % 1000);
  }
  bug(33);
  return;
}

/*
  Function to scan a file up to a specified
  point and either print or return a string.
*/
int rdupto(fdi, uptoc, print, string)
FILE *fdi;
char uptoc, print, *string;
{
  int c;

  while ((c = fgetc(fdi)) != uptoc) {
	if (c == EOF) return(0);
	if (c == '\r') continue;
	if (print)
		fputc(c, stdout);
	else
		*string++ = c;
  }
  if (!print) *string = '\0';
  return(1);
}

/*
  Function to read a file skipping
  a given character a specified number
  of times, with or without repositioning
  the file.
*/
void rdskip(fdi, skipc, n, rewind_)
FILE *fdi;
char skipc, rewind_;
int n;
{
  int c;

  if (rewind_)
	if (fseek(fdi, 0, 0) == -1) bug(31);
  while (n--) while ((c = fgetc(fdi)) != skipc)
		if (c == EOF) bug(32);
}

/*
  Routine to request a yes or no answer to a question.
*/
int yes(msg1, msg2, msg3)
int msg1, msg2, msg3;
{
  char answer[80];

  if (msg1) rspeak(msg1);
  fprintf(stdout, "\n> ");
  fgets(answer, 80, stdin);
  if (tolower(answer[0]) == 'n') {
	if (msg3) rspeak(msg3);
	return(0);
  }
  if (msg2) rspeak(msg2);
  return(1);
}

/*
  Print a location description from "advent4.txt"
*/
void rspeak(msg)
int msg;
{
  if (msg == 54)
	printf("ok.\n");
  else {
	if (g.dbugflg) printf("Seek loc msg #%d @ %ld\n", msg, idx4[msg]);
	fseek(fd4, idx4[msg - 1], 0);
	rdupto(fd4, '#', 1, 0);
  }
  return;
}

/*
  Print an item message for a given state from "advent3.txt"
*/
void pspeak(item, state)
int item, state;
{
  fseek(fd3, idx3[item - 1], 0);
  rdskip(fd3, '/', state + 2, 0);
  rdupto(fd3, '/', 1, 0);
}

/*
  Print a long location description from "advent1.txt"
*/
void desclg(loc)
int loc;
{
  fseek(fd1, idx1[loc - 1], 0);
  rdupto(fd1, '#', 1, 0);
}

/*
  Print a short location description from "advent2.txt"
*/
void descsh(loc)
int loc;
{
  fseek(fd2, idx2[loc - 1], 0);
  rdupto(fd2, '#', 1, 0);
}

/*
  look-up vocabulary word in lex-ordered table.  words may have
  two entries with different codes. if minimum acceptable value
  = 0, then return minimum of different codes.  last word CANNOT
  have two entries(due to binary sort).
  word is the word to look up.
  val  is the minimum acceptable value,
	if != 0 return %1000
*/
int vocab(word, val)
char *word;
int val;
{
  int v1, v2;

  if ((v1 = binary(word, wc, MAXWC)) >= 0) {
	if (v1 > 0 && strcmp(word, wc[v1 - 1].aword) == 0)
		v2 = v1 - 1;
	else if (v1 < (MAXWC-1) && strcmp(word, wc[v1 + 1].aword) == 0)
		v2 = v1 + 1;
	else
		v2 = v1;
	if (!val) return(wc[v1].acode < wc[v2].acode
			? wc[v1].acode : wc[v2].acode);
	if (val <= wc[v1].acode)
		return(wc[v1].acode % 1000);
	else if (val <= wc[v2].acode)
		return(wc[v2].acode % 1000);
	else
		return(-1);
  } else
	return(-1);
}

int binary(w, wctable, maxwc)
char *w;
int maxwc;
struct wac wctable[];
{
  int lo, mid, hi, check;

  lo = 0;
  hi = maxwc - 1;
  while (lo <= hi) {
	mid = (lo + hi) / 2;
	if ((check = strcmp(w, wctable[mid].aword)) < 0)
		hi = mid - 1;
	else if (check > 0)
		lo = mid + 1;
	else
		return(mid);
  }
  return(-1);
}


/*
  Utility Routines
*/

/*
  Routine to test for darkness
*/
int dark()
{
  return(!(g.cond[g.loc] & LIGHT) &&
	(!g.prop[LAMP] ||
	 !here(LAMP)));
}

/*
  Routine to tell if an item is present.
*/
int here(item)
int item;
{
  return(g.place[item] == g.loc || toting(item));
}

/*
  Routine to tell if an item is being carried.
*/
int toting(item)
int item;
{
  return(g.place[item] == -1);
}

/*
  Routine to tell if a location causes
  a forced move.
*/
int forced(atloc)
int atloc;
{
  return(g.cond[atloc] == 2);
}

/*
  Routine true x% of the time.
*/
int pct(x)
int x;
{
  return(rand() % 100 < x);
}

/*
  Routine to tell if player is on
  either side of a two sided object.
*/
int at(item)
int item;
{
  return(g.place[item] == g.loc || g.fixed[item] == g.loc);
}

/*
  Routine to destroy an object
*/
void dstroy(obj)
int obj;
{
  move(obj, 0);
}

/*
  Routine to move an object
*/
void move(obj, where)
int obj, where;
{
  int from;

  from = (obj < MAXOBJ) ? g.place[obj] : g.fixed[obj];
  if (from > 0 && from <= 300) carry(obj, from);
  drop(obj, where);
}

/*
  Juggle an object
  currently a no-op
*/
void juggle(loc)
int loc;
{
}

/*
  Routine to carry an object
*/
void carry(obj, where)
int obj, where;
{
  if (obj < MAXOBJ) {
	if (g.place[obj] == -1) return;
	g.place[obj] = -1;
	++g.holding;
  }
}

/*
  Routine to drop an object
*/
void drop(obj, where)
int obj, where;
{
  if (obj < MAXOBJ) {
	if (g.place[obj] == -1) --g.holding;
	g.place[obj] = where;
  } else
	g.fixed[obj - MAXOBJ] = where;
}

/*
  routine to move an object and return a
  value used to set the negated prop values
  for the repository.
*/
int put(obj, where, pval)
int obj, where, pval;
{
  move(obj, where);
  return((-1) - pval);
}

/*
  Routine to check for presence
  of dwarves..
*/
int dcheck()
{
  int i;

  for (i = 1; i < (DWARFMAX - 1); ++i)
	if (g.dloc[i] == g.loc) return(i);
  return(0);
}

/*
  Determine liquid in the bottle
*/
int liq()
{
  int i, j;
  i = g.prop[BOTTLE];
  j = -1 - i;
  return(liq2(i > j ? i : j));
}

/*
  Determine liquid at a location
*/
int liqloc(loc)
int loc;
{
  if (g.cond[loc] & LIQUID)
	return(liq2(g.cond[loc] & WATOIL));
  else
	return(liq2(1));
}

/*
  Convert  0 to WATER
	 1 to nothing
	 2 to OIL
*/
int liq2(pbottle)
int pbottle;
{
  return((1 - pbottle) * WATER + (pbottle >> 1) * (WATER + OIL));
}

/*
  Fatal error routine
*/
void bug(n)
int n;
{
  printf("Fatal error number %d\n", n);
  exit(1);
}
