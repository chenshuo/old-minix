
/*	header ADVDEC.H						*\
\*	WARNING: GLOBAL EXTERNAL declarations for adventure	*/


/*
  Database variables
*/
extern struct wac wc[];		/* see ADVWORD.H		 */
extern long idx1[];		/* see ADVTEXT.H		 */
extern long idx2[];		/* see ADVTEXT.H		 */
extern long idx3[];		/* see ADVTEXT.H		 */
extern long idx4[];		/* see ADVTEXT.H		 */



extern struct trav travel[];
extern FILE *fd1, *fd2, *fd3, *fd4;
extern int actmsg[];		/* action messages	 */

/*
  English variables
*/
extern int verb, object, motion;
extern char word1[], word2[];

/*
  Play variables
*/
extern struct {
  int turns;
  int loc, oldloc, oldloc2, newloc;	/* location variables */
  int cond[MAXLOC];		/* location status	 */
  int place[MAXOBJ];		/* object location	 */
  int fixed[MAXOBJ];		/* second object loc	 */
  int visited[MAXLOC];		/* >0 if has been here	 */
  int prop[MAXOBJ];		/* status of object	 */
  int tally, tally2;		/* item counts		 */
  int limit;			/* time limit		 */
  int lmwarn;			/* lamp warning flag	 */
  int wzdark, closing, closed;	/* game state flags	 */
  int holding;			/* count of held items	 */
  int detail;			/* LOOK count		 */
  int knfloc;			/* knife location	 */
  int clock, clock2, panic;	/* timing variables	 */
  int dloc[DWARFMAX];		/* dwarf locations	 */
  int dflag;			/* dwarf flag		 */
  int dseen[DWARFMAX];		/* dwarf seen flag	 */
  int odloc[DWARFMAX];		/* dwarf old locations	 */
  int daltloc;			/* alternate appearance	 */
  int dkill;			/* dwarves killed	 */
  int chloc, chloc2;		/* chest locations	 */
  int bonus;			/* to pass to end	 */
  int numdie;			/* number of deaths	 */
  int object1;			/* to help intrans.	 */
  int gaveup;			/* 1 if he quit early	 */
  int foobar;			/* fee fie foe foo...	 */
  int saveflg;			/* if game being saved	 */
  int dbugflg;			/* if game is in debug	 */


  int lastglob;			/* to get space req.	 */
} g;
