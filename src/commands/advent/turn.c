
/*	program TURN.C						*
 *	WARNING: "advent.c" allocates GLOBAL storage space by	*
 *		including "advdef.h".				*
 *		All other modules use "advdec.h".		*/


#include	"stdio.h"
#include	"advent.h"
#include	"advdec.h"

_PROTOTYPE (void exit, (int status));

/*
  Routine to take 1 turn
*/
void turn()
{
  int i;
  /* If closing, then he can't leave except via the main office. */
  if (g.newloc < 9 && g.newloc != 0 && g.closing) {
	rspeak(130);
	g.newloc = g.loc;
	if (!g.panic) g.clock2 = 15;
	g.panic = 1;
  }

  /* See if a dwarf has seen him and has come from where he wants to go. */
  if (g.newloc != g.loc && !forced(g.loc) && g.cond[g.loc] & NOPIRAT == 0)
	for (i = 1; i < (DWARFMAX - 1); ++i)
		if (g.odloc[i] == g.newloc && g.dseen[i]) {
			g.newloc = g.loc;
			rspeak(2);
			break;
		}
  dwarves();			/* & special dwarf(pirate who steals)	 */

  /* Added by BDS C conversion */
  if (g.loc != g.newloc) {
	++g.turns;
	g.loc = g.newloc;
/*	causes occasional "move" with two describe & descitem	*/
	/*		}	*//* if (loc != newloc)	 */

	/* Check for death */
	if (g.loc == 0) {
		death();
		return;
	}

	/* Check for forced move */
	if (forced(g.loc)) {
		describe();
		domove();
		return;
	}

	/* Check for wandering in dark */
	if (g.wzdark && dark() && pct(35)) {
		rspeak(23);
		g.oldloc2 = g.loc;
		death();
		return;
	}

	/* Describe his situation */
	describe();
	if (!dark()) {
		++g.visited[g.loc];
		descitem();
	}

/*	causes occasional "move" with no describe & descitem	*/
  }				/* if (loc != newloc)	 */
  if (g.closed) {
	if (g.prop[OYSTER] < 0 && toting(OYSTER)) pspeak(OYSTER, 1);
	for (i = 1; i <= MAXOBJ; ++i)
		if (toting(i) && g.prop[i] < 0) g.prop[i] = -1 - g.prop[i];
  }
  g.wzdark = dark();
  if (g.knfloc > 0 && g.knfloc != g.loc) g.knfloc = 0;

  if (stimer())			/* as the grains of sand slip by	 */
	return;

  while (!english())		/* retrieve player instructions	 */
	;

  if (g.dbugflg) printf("loc = %d, verb = %d, object = %d, \
	motion = %d\n", g.loc, verb, object, motion);

  if (motion)			/* execute player instructions	 */
	domove();
  else if (object)
	doobj();
  else
	itverb();
}

/*
  Routine to describe current location
*/
void describe()
{
  if (toting(BEAR)) rspeak(141);
  if (dark())
	rspeak(16);
  else if (g.visited[g.loc])
	descsh(g.loc);
  else
	desclg(g.loc);
  if (g.loc == 33 && pct(25) && !g.closing) rspeak(8);
}

/*
  Routine to describe visible items
*/
void descitem()
{
  int i, state;

  for (i = 1; i < MAXOBJ; ++i) {
	if (at(i)) {
		if (i == STEPS && toting(NUGGET)) continue;
		if (g.prop[i] < 0) {
			if (g.closed)
				continue;
			else {
				g.prop[i] = 0;
				if (i == RUG || i == CHAIN) ++g.prop[i];
				--g.tally;
			}
		}
		if (i == STEPS && g.loc == g.fixed[STEPS])
			state = 1;
		else
			state = g.prop[i];
		pspeak(i, state);
	}
  }
  if (g.tally == g.tally2 && g.tally != 0 && g.limit > 35) g.limit = 35;
}

/*
  Routine to handle motion requests
*/
void domove()
{
  gettrav(g.loc);
  switch (motion) {
      case NULLX:
	break;
      case BACK:	goback();	break;
      case LOOK:
	if (g.detail++ < 3) rspeak(15);
	g.wzdark = 0;
	g.visited[g.loc] = 0;
	g.newloc = g.loc;
	g.loc = 0;
	break;
      case CAVE:
	if (g.loc < 8)
		rspeak(57);
	else
		rspeak(58);
	break;
      default:
	g.oldloc2 = g.oldloc;
	g.oldloc = g.loc;
	dotrav();
  }
}

/*
  Routine to handle request to return
  from whence we came!
*/
void goback()
{
  int kk, k2, want, temp;
  struct trav strav[MAXTRAV];

  if (forced(g.oldloc))
	want = g.oldloc2;
  else
	want = g.oldloc;
  g.oldloc2 = g.oldloc;
  g.oldloc = g.loc;
  k2 = 0;
  if (want == g.loc) {
	rspeak(91);
	return;
  }
  copytrv(travel, strav);
  for (kk = 0; travel[kk].tdest != -1; ++kk) {
	if (!travel[kk].tcond && travel[kk].tdest == want) {
		motion = travel[kk].tverb;
		dotrav();
		return;
	}
	if (!travel[kk].tcond) {
		k2 = kk;
		temp = travel[kk].tdest;
		gettrav(temp);
		if (forced(temp) && travel[0].tdest == want) k2 = temp;
		copytrv(strav, travel);
	}
  }
  if (k2) {
	motion = travel[k2].tverb;
	dotrav();
  } else
	rspeak(140);
}

/*
  Routine to copy a travel array
*/
void copytrv(trav1, trav2)
struct trav *trav1, *trav2;
{
  int i;

  for (i = 0; i < MAXTRAV; ++i) {
	trav2->tdest = trav1->tdest;
	trav2->tverb = trav1->tverb;
	trav2->tcond = trav1->tcond;
  }
}

/*
  Routine to figure out a new location
  given current location and a motion.
*/
void dotrav()
{
  char mvflag, hitflag;
  int kk;
  int rdest, rverb, rcond, robject;
  int pctt;

  g.newloc = g.loc;
  mvflag = hitflag = 0;
  pctt = rand() % 100;

  for (kk = 0; travel[kk].tdest >= 0 && !mvflag; ++kk) {
	rdest = travel[kk].tdest;
	rverb = travel[kk].tverb;
	rcond = travel[kk].tcond;
	robject = rcond % 100;

	if (g.dbugflg) printf("rdest = %d, rverb = %d, rcond = %d, \
		robject = %d in dotrav\n", rdest, rverb,
		       rcond, robject);
	if ((rverb != 1) && (rverb != motion) && !hitflag) continue;
	++hitflag;
	switch (rcond / 100) {
	    case 0:
		if ((rcond == 0) || (pctt < rcond)) ++mvflag;
		if (rcond && g.dbugflg) printf("%% move %d %d\n",
			       pctt, mvflag);
		break;
	    case 1:
		if (robject == 0)
			++mvflag;
		else if (toting(robject))
			++mvflag;
		break;
	    case 2:
		if (toting(robject) || at(robject)) ++mvflag;
		break;
	    case 3:
	    case 4:
	    case 5:
	    case 7:
		if (g.prop[robject] != (rcond / 100) - 3) ++mvflag;
		break;
	    default:	bug(37);
	}
  }
  if (!mvflag)
	badmove();
  else if (rdest > 500)
	rspeak(rdest - 500);
  else if (rdest > 300)
	spcmove(rdest);
  else {
	g.newloc = rdest;
	if (g.dbugflg) printf("newloc in dotrav = %d\n", g.newloc);
  }
}

/*
  The player tried a poor move option.
*/
void badmove()
{
  int msg;

  msg = 12;
  if (motion >= 43 && motion <= 50) msg = 9;
  if (motion == 29 || motion == 30) msg = 9;
  if (motion == 7 || motion == 36 || motion == 37) msg = 10;
  if (motion == 11 || motion == 19) msg = 11;
  if (verb == FIND || verb == INVENTORY) msg = 59;
  if (motion == 62 || motion == 65) msg = 42;
  if (motion == 17) msg = 80;
  rspeak(msg);
}

/*
  Routine to handle very special movement.
*/
void spcmove(rdest)
int rdest;
{
  switch (rdest - 300) {
      case 1:			/* plover movement via alcove */
	if (!g.holding || (g.holding == 1 && toting(EMERALD)))
		g.newloc = (99 + 100) - g.loc;
	else
		rspeak(117);
	break;
      case 2:			/* trying to remove plover, bad route */
	drop(EMERALD, g.loc);
	break;
      case 3:			/* troll bridge */
	if (g.prop[TROLL] == 1) {
		pspeak(TROLL, 1);
		g.prop[TROLL] = 0;
		move(TROLL2, 0);
		move((TROLL2 + MAXOBJ), 0);
		move(TROLL, 117);
		move((TROLL + MAXOBJ), 122);
		juggle(CHASM);
		g.newloc = g.loc;
	} else {
		g.newloc = (g.loc == 117 ? 122 : 117);
		if (g.prop[TROLL] == 0) ++g.prop[TROLL];
		if (!toting(BEAR)) return;
		rspeak(162);
		g.prop[CHASM] = 1;
		g.prop[TROLL] = 2;
		drop(BEAR, g.newloc);
		g.fixed[BEAR] = -1;
		g.prop[BEAR] = 3;
		if (g.prop[SPICES] < 0) ++g.tally2;
		g.oldloc2 = g.newloc;
		death();
	}
	break;
      default:	bug(38);
}
}


/*
  Routine to handle player's demise via
  waking up the dwarves...
*/
void dwarfend()
{
  death();
  normend();
}

/*
  normal end of game
*/
void normend()
{
  score();
  exit(1);
}

/*
  scoring
*/
void score()
{
  int t, i, k, s;
  s = 0;
  for (i = 50; i <= MAXTRS; ++i) {
	if (i == CHEST)
		k = 14;
	else if (i > CHEST)
		k = 16;
	else
		k = 12;
	if (g.prop[i] >= 0) s += 2;
	if (g.place[i] == 3 && g.prop[i] == 0) s += k - 2;
  }
  printf("%-20s%d\n", "Treasures found:", 15 - g.tally);
  s -= g.numdie * 10;
  if (g.numdie) printf("%-20s%d\n", "Deaths:", g.numdie);
  if (g.gaveup) s -= 4;
  t = g.dflag ? 25 : 0;
  s += t;
  t = g.closing ? 25 : 0;
  if (t) printf("%-20s%d\n", "Masters section:", t);
  s += t;
  if (g.closed) {
	if (g.bonus == 0)
		t = 10;
	else if (g.bonus == 135)
		t = 25;
	else if (g.bonus == 134)
		t = 30;
	else if (g.bonus == 133)
		t = 45;
	printf("%-20s%d\n", "Bonus:", t);
	s += t;
  }
  if (g.place[MAGAZINE] == 108) s += 1;
  printf("%-20s%d\n", "Score:", s);
}

/*
  Routine to handle the passing on of one
  of the player's incarnations...
*/
void death()
{
  char yea, i, j;

  if (!g.closing) {
	yea = yes(81 + g.numdie * 2, 82 + g.numdie * 2, 54);
	if (++g.numdie >= MAXDIE || !yea) normend();
	g.place[WATER] = 0;
	g.place[OIL] = 0;
	if (toting(LAMP)) g.prop[LAMP] = 0;
	for (j = 1; j < 101; ++j) {
		i = 101 - j;
		if (toting(i)) drop(i, i == LAMP ? 1 : g.oldloc2);
	}
	g.newloc = 3;
	g.oldloc = g.loc;
	return;
  }

  /* Closing -- no resurrection... */
  rspeak(131);
  ++g.numdie;
  normend();
}

/*
  Routine to print word corresponding to object
*/
char *
probj(object)
int object;
{
  int wtype, wval;
  analyze(word1, &wtype, &wval);
  return(wtype == 1 ? word1 : word2);
}

/*
  Routine to process an object.
*/
void doobj()
{
  /* Is object here?  if so, transitive */
  if (g.fixed[object] == g.loc || here(object)) trobj();
  /* Did he give grate as destination? */
  else if (object == GRATE) {
	if (g.loc == 1 || g.loc == 4 || g.loc == 7) {
		motion = DEPRESSION;
		domove();
	} else if (g.loc > 9 && g.loc < 15) {
		motion = ENTRANCE;
		domove();
	}
  }

  /* Is it a dwarf he is after? */
  else if (dcheck() && g.dflag >= 2) {
	object = DWARF;
	trobj();
  }

  /* Is he trying to get/use a liquid? */
  else if ((liq() == object && here(BOTTLE)) ||
	 liqloc(g.loc) == object)
	trobj();
  else if (object == PLANT && at(PLANT2) &&
	 g.prop[PLANT2] == 0) {
	object = PLANT2;
	trobj();
  }

  /* Is he trying to grab a knife? */
  else if (object == KNIFE && g.knfloc == g.loc) {
	rspeak(116);
	g.knfloc = -1;
  }

  /* Is he trying to get at dynamite? */
  else if (object == ROD && here(ROD2)) {
	object = ROD2;
	trobj();
  } else
	printf("I see no %s here.\n", probj(object));
}

/*
  Routine to process an object being
  referred to.
*/
void trobj()
{
  if (verb)
	trverb();
  else
	printf("What do you want to do with the %s?\n",
	       probj(object));
}

/*
  dwarf stuff.
*/
void dwarves()
{
  int i, j, k, try, attack, stick, dtotal;
  /* See if dwarves allowed here */
  if (g.newloc == 0 || forced(g.newloc) || g.cond[g.newloc] & NOPIRAT)
	return;
  /* See if dwarves are active. */
  if (!g.dflag) {
	if (g.newloc > 15) ++g.dflag;
	return;
  }

  /* If first close encounter (of 3rd kind) kill 0, 1 or 2 */
  if (g.dflag == 1) {
	if (g.newloc < 15 || pct(95)) return;
	++g.dflag;
	for (i = 1; i < 3; ++i)
		if (pct(50)) g.dloc[rand() % 5 + 1] = 0;
	for (i = 1; i < (DWARFMAX - 1); ++i) {
		if (g.dloc[i] == g.newloc) g.dloc[i] = g.daltloc;
		g.odloc[i] = g.dloc[i];
	}
	rspeak(3);
	drop(AXE, g.newloc);
	return;
  }
  dtotal = attack = stick = 0;
  for (i = 1; i < DWARFMAX; ++i) {
	if (g.dloc[i] == 0) continue;
	/* Move a dwarf at random.  we don't have a matrix around to
	 * do it as in the original version... */
	for (try = 1; try < 20; ++try) {
		j = rand() % 106 + 15;	/* allowed area */
		if (j != g.odloc[i] && j != g.dloc[i] &&
		 !(i == (DWARFMAX - 1) && g.cond[j] & NOPIRAT == 1))
			break;
	}
	if (j == 0) j = g.odloc[i];
	g.odloc[i] = g.dloc[i];
	g.dloc[i] = j;
	if ((g.dseen[i] && g.newloc >= 15) ||
	    g.dloc[i] == g.newloc || g.odloc[i] == g.newloc)
		g.dseen[i] = 1;
	else
		g.dseen[i] = 0;
	if (!g.dseen[i]) continue;
	g.dloc[i] = g.newloc;
	if (i == 6)
		dopirate();
	else {
		++dtotal;
		if (g.odloc[i] == g.dloc[i]) {
			++attack;
			if (g.knfloc >= 0) g.knfloc = g.newloc;
			if (rand() % 1000 < 45 * (g.dflag - 2)) ++stick;
		}
	}
  }
  if (dtotal == 0) return;
  if (dtotal > 1)
	printf("There are %d threatening little dwarves in the room with you!\n", dtotal);
  else
	rspeak(4);
  if (attack == 0) return;
  if (g.dflag == 2) ++g.dflag;
  if (attack > 1) {
	printf("%d of them throw knives at you!!\n", attack);
	k = 6;
  } else {
	rspeak(5);
	k = 52;
  }
  if (stick <= 1) {
	rspeak(stick + k);
	if (stick == 0) return;
  } else
	printf("%d of them get you !!!\n", stick);
  g.oldloc2 = g.newloc;
  death();
}

/*
  pirate stuff
*/
void dopirate()
{
  int j, k;
  if (g.newloc == g.chloc || g.prop[CHEST] >= 0) return;
  k = 0;
  for (j = 50; j <= MAXTRS; ++j)
	if (j != PYRAMID ||
	    (g.newloc != g.place[PYRAMID] &&
	     g.newloc != g.place[EMERALD])) {
		if (toting(j)) goto stealit;
		if (here(j)) ++k;
	}
  if (g.tally == g.tally2 + 1 && k == 0 && g.place[CHEST] == 0 &&
      here(LAMP) && g.prop[LAMP] == 1) {
	rspeak(186);
	move(CHEST, g.chloc);
	move(MESSAGE, g.chloc2);
	g.dloc[6] = g.chloc;
	g.odloc[6] = g.chloc;
	g.dseen[6] = 0;
	return;
  }
  if (g.odloc[6] != g.dloc[6] && pct(20)) {
	rspeak(127);
	return;
  }
  return;

stealit:

  rspeak(128);
  if (g.place[MESSAGE] == 0) move(CHEST, g.chloc);
  move(MESSAGE, g.chloc2);
  for (j = 50; j <= MAXTRS; ++j) {
	if (j == PYRAMID &&
	    (g.newloc == g.place[PYRAMID] ||
	     g.newloc == g.place[EMERALD]))
		continue;
	if (at(j) && g.fixed[j] == 0) carry(j, g.newloc);
	if (toting(j)) drop(j, g.chloc);
  }
  g.dloc[6] = g.chloc;
  g.odloc[6] = g.chloc;
  g.dseen[6] = 0;
}

/*
  special time limit stuff...
*/
int stimer()
{
  int i;
  g.foobar = g.foobar > 0 ? -g.foobar : 0;
  if (g.tally == 0 && g.loc >= 15 && g.loc != 33) --g.clock;
  if (g.clock == 0) {
	/* Start closing the cave */
	g.prop[GRATE] = 0;
	g.prop[FISSURE] = 0;
	for (i = 1; i < DWARFMAX; ++i) g.dseen[i] = 0;
	move(TROLL, 0);
	move((TROLL + MAXOBJ), 0);
	move(TROLL2, 117);
	move((TROLL2 + MAXOBJ), 122);
	juggle(CHASM);
	if (g.prop[BEAR] != 3) dstroy(BEAR);
	g.prop[CHAIN] = 0;
	g.fixed[CHAIN] = 0;
	g.prop[AXE] = 0;
	g.fixed[AXE] = 0;
	rspeak(129);
	g.clock = -1;
	g.closing = 1;
	return(0);
  }
  if (g.clock < 0) --g.clock2;
  if (g.clock2 == 0) {
	/* Set up storage room... and close the cave... */
	g.prop[BOTTLE] = put(BOTTLE, 115, 1);
	g.prop[PLANT] = put(PLANT, 115, 0);
	g.prop[OYSTER] = put(OYSTER, 115, 0);
	g.prop[LAMP] = put(LAMP, 115, 0);
	g.prop[ROD] = put(ROD, 115, 0);
	g.prop[DWARF] = put(DWARF, 115, 0);
	g.loc = 115;
	g.oldloc = 115;
	g.newloc = 115;
	put(GRATE, 116, 0);
	g.prop[SNAKE] = put(SNAKE, 116, 1);
	g.prop[BIRD] = put(BIRD, 116, 1);
	g.prop[CAGE] = put(CAGE, 116, 0);
	g.prop[ROD2] = put(ROD2, 116, 0);
	g.prop[PILLOW] = put(PILLOW, 116, 0);
	g.prop[MIRROR] = put(MIRROR, 115, 0);
	g.fixed[MIRROR] = 116;
	for (i = 1; i <= MAXOBJ; ++i)
		if (toting(i)) dstroy(i);
	rspeak(132);
	g.closed = 1;
	return(1);
  }
  if (g.prop[LAMP] == 1) --g.limit;
  if (g.limit <= 30 &&
      here(BATTERIES) && g.prop[BATTERIES] == 0 &&
      here(LAMP)) {
	rspeak(188);
	g.prop[BATTERIES] = 1;
	if (toting(BATTERIES)) drop(BATTERIES, g.loc);
	g.limit += 2500;
	g.lmwarn = 0;
	return(0);
  }
  if (g.limit == 0) {
	--g.limit;
	g.prop[LAMP] = 0;
	if (here(LAMP)) rspeak(184);
	return(0);
  }
  if (g.limit < 0 && g.loc <= 8) {
	rspeak(185);
	g.gaveup = 1;
	normend();
  }
  if (g.limit <= 30) {
	if (g.lmwarn || !here(LAMP)) return(0);
	g.lmwarn = 1;
	i = 187;
	if (g.place[BATTERIES] == 0) i = 183;
	if (g.prop[BATTERIES] == 1) i = 189;
	rspeak(i);
	return(0);
  }
  return(0);
}

/*
  random number seed
*/
static long rnum = 0;

void srand(n)
short n;
{
  rnum = (long) n;
}

/*
  random number
*/
int rand()
{
  rnum = rnum * 0x41C64E6D + 0x3039;
  return((short) (rnum >> 16) & 0x7FFF);
}
