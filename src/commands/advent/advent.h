/*	header ADVENT.H						*\
\*	WARNING: HEADER file for all adventure modules		*/


#define	MAXOBJ	100		/* max # of objects in cave	 */
#define	MAXWC	301		/* max # of adventure words	 */
#define	MAXLOC	140		/* max # of cave locations	 */
#define	WORDSIZE	20	/* max # of chars in commands	 */
#define	MAXMSG	201		/* max # of long location descr	 */

#define	MAXTRAV	(16+1)		/* max # of travel directions from loc	 */
 /* +1 for terminator travel[x].tdest=-1	 */
#define	DWARFMAX	7	/* max # of nasty dwarves	 */
#define	MAXDIE	3		/* max # of deaths before close	 */
#define	MAXTRS	79		/* max # of			 */

/*
  Object definitions
*/
#define	KEYS	1
#define	LAMP	2
#define	GRATE	3
#define	CAGE	4
#define	ROD	5
#define	ROD2	6
#define	STEPS	7
#define	BIRD	8
#define	DOOR	9
#define	PILLOW	10
#define	SNAKE	11
#define	FISSURE	12
#define	TABLET	13
#define	CLAM	14
#define	OYSTER	15
#define	MAGAZINE	16
#define	DWARF	17
#define	KNIFE	18
#define	FOOD	19
#define	BOTTLE	20
#define	WATER	21
#define	OIL	22
#define	MIRROR	23
#define	PLANT	24
#define	PLANT2	25
#define	AXE	28
#define	DRAGON	31
#define	CHASM	32
#define	TROLL	33
#define	TROLL2	34
#define	BEAR	35
#define	MESSAGE	36
#define	VEND	38
#define	BATTERIES	39
#define	NUGGET	50
#define	COINS	54
#define	CHEST	55
#define	EGGS	56
#define	TRIDENT	57
#define	VASE	58
#define	EMERALD	59
#define	PYRAMID	60
#define	PEARL	61
#define	RUG	62
#define	SPICES	63
#define	CHAIN	64

/*
  Verb definitions
*/
#define	NULLX	21
#define	BACK	8
#define	LOOK	57
#define	CAVE	67
#define	ENTRANCE	64
#define	DEPRESSION	63

/*
  Action verb definitions
*/
#define	TAKE	1
#define	DROP	2
#define	SAY	3
#define	OPEN	4
#define	NOTHING	5
#define	LOCK	6
#define	ON	7
#define	OFF	8
#define	WAVE	9
#define	CALM	10
#define	WALK	11
#define	KILL	12
#define	POUR	13
#define	EAT	14
#define	DRINK	15
#define	RUB	16
#define	THROW	17
#define	QUIT	18
#define	FIND	19
#define	INVENTORY	20
#define	FEED	21
#define	FILL	22
#define	BLAST	23
#define	SCORE	24
#define	FOO	25
#define	BRIEF	26
#define	READ	27
#define	BREAK	28
#define	WAKE	29
#define	SUSPEND	30
#define	HOURS	31
#define	LOG	32

/*
  BIT mapping of "cond" array which indicates location status
*/
#define	LIGHT	1
#define	WATOIL	2
#define	LIQUID	4
#define	NOPIRAT	8
#define	HINTC	16
#define	HINTB	32
#define	HINTS	64
#define	HINTM	128
#define	HINT	240

/*
  Structure definitions
*/
struct wac {
  char *aword;
  int acode;
};

struct trav {
  int tdest;
  int tverb;
  int tcond;
};

/* function prototypes */

/* advent.c */

_PROTOTYPE(int main, (int argc, char **argv ));
_PROTOTYPE(void scanint, (int *pi, char *str ));
_PROTOTYPE(void initplay, (void));
_PROTOTYPE(void opentxt, (void));
_PROTOTYPE(void saveadv, (void));
_PROTOTYPE(void restore, (void));

/* advent0.c */

_PROTOTYPE(int main, (int argc, char **argv ));

/* database.c */

_PROTOTYPE(void gettrav, (int loc ));
_PROTOTYPE(int rdupto, (FILE *fdi, int uptoc, int print, char *string ));
_PROTOTYPE(void rdskip, (FILE *fdi, int skipc, int n, int rewind_ ));
_PROTOTYPE(int yes, (int msg1, int msg2, int msg3 ));
_PROTOTYPE(void rspeak, (int msg ));
_PROTOTYPE(void pspeak, (int item, int state ));
_PROTOTYPE(void desclg, (int loc ));
_PROTOTYPE(void descsh, (int loc ));
_PROTOTYPE(int vocab, (char *word, int val ));
_PROTOTYPE(int binary, (char *w, struct wac wctable [], int maxwc ));
_PROTOTYPE(int dark, (void));
_PROTOTYPE(int here, (int item ));
_PROTOTYPE(int toting, (int item ));
_PROTOTYPE(int forced, (int atloc ));
_PROTOTYPE(int pct, (int x ));
_PROTOTYPE(int at, (int item ));
_PROTOTYPE(void dstroy, (int obj ));
_PROTOTYPE(void move, (int obj, int where ));
_PROTOTYPE(void juggle, (int loc ));
_PROTOTYPE(void carry, (int obj, int where ));
_PROTOTYPE(void drop, (int obj, int where ));
_PROTOTYPE(int put, (int obj, int where, int pval ));
_PROTOTYPE(int dcheck, (void));
_PROTOTYPE(int liq, (void));
_PROTOTYPE(int liqloc, (int loc ));
_PROTOTYPE(int liq2, (int pbottle ));
_PROTOTYPE(void bug, (int n ));

/* english.c */

_PROTOTYPE(int english, (void));
_PROTOTYPE(int analyze, (char *word, int *type, int *value ));
_PROTOTYPE(void getwords, (void));
_PROTOTYPE(void outwords, (void));

/* itverb.c */

_PROTOTYPE(void itverb, (void));
_PROTOTYPE(void ivtake, (void));
_PROTOTYPE(void ivopen, (void));
_PROTOTYPE(void ivkill, (void));
_PROTOTYPE(void ivdrink, (void));
_PROTOTYPE(void ivquit, (void));
_PROTOTYPE(void ivfoo, (void));
_PROTOTYPE(void inventory, (void));
_PROTOTYPE(void addobj, (int obj ));

/* turn.c */

_PROTOTYPE(void turn, (void));
_PROTOTYPE(void describe, (void));
_PROTOTYPE(void descitem, (void));
_PROTOTYPE(void domove, (void));
_PROTOTYPE(void goback, (void));
_PROTOTYPE(void copytrv, (struct trav *trav1, struct trav *trav2 ));
_PROTOTYPE(void dotrav, (void));
_PROTOTYPE(void badmove, (void));
_PROTOTYPE(void spcmove, (int rdest ));
_PROTOTYPE(void dwarfend, (void));
_PROTOTYPE(void normend, (void));
_PROTOTYPE(void score, (void));
_PROTOTYPE(void death, (void));
_PROTOTYPE(char *probj, (int object ));
_PROTOTYPE(void doobj, (void));
_PROTOTYPE(void trobj, (void));
_PROTOTYPE(void dwarves, (void));
_PROTOTYPE(void dopirate, (void));
_PROTOTYPE(int stimer, (void));
_PROTOTYPE(void srand, (int n ));
_PROTOTYPE(int rand, (void));

/* verb.c */

_PROTOTYPE(void trverb, (void));
_PROTOTYPE(void vtake, (void));
_PROTOTYPE(void vdrop, (void));
_PROTOTYPE(void vopen, (void));
_PROTOTYPE(void vsay, (void));
_PROTOTYPE(void von, (void));
_PROTOTYPE(void voff, (void));
_PROTOTYPE(void vwave, (void));
_PROTOTYPE(void vkill, (void));
_PROTOTYPE(void vpour, (void));
_PROTOTYPE(void veat, (void));
_PROTOTYPE(void vdrink, (void));
_PROTOTYPE(void vthrow, (void));
_PROTOTYPE(void vfind, (void));
_PROTOTYPE(void vfill, (void));
_PROTOTYPE(void vfeed, (void));
_PROTOTYPE(void vread, (void));
_PROTOTYPE(void vblast, (void));
_PROTOTYPE(void vbreak, (void));
_PROTOTYPE(void vwake, (void));
_PROTOTYPE(void actspk, (int verb ));
_PROTOTYPE(void needobj, (void));
