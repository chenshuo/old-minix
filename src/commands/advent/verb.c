
/**	program VERB.C						*
 *	WARNING: "advent.c" allocates GLOBAL storage space by	*
 *		including "advdef.h".				*
 *		All other modules use "advdec.h".		*/


#include	"stdio.h"
#include	"advent.h"
#include	"advdec.h"


/*
  Routine to process a transitive verb
*/
void trverb()
{
  switch (verb) {
      case CALM:
      case WALK:
      case QUIT:
      case SCORE:
      case FOO:
      case BRIEF:
      case SUSPEND:
      case HOURS:
      case LOG:	actspk(verb);	break;
      case TAKE:	vtake();	break;
      case DROP:	vdrop();	break;
      case OPEN:
      case LOCK:	vopen();	break;
      case SAY:	vsay();	break;
      case NOTHING:	rspeak(54);	break;
      case ON:	von();	break;
      case OFF:	voff();	break;
      case WAVE:	vwave();	break;
      case KILL:	vkill();	break;
      case POUR:	vpour();	break;
      case EAT:	veat();	break;
      case DRINK:	vdrink();	break;
      case RUB:
	if (object != LAMP)
		rspeak(76);
	else
		actspk(RUB);
	break;
      case THROW:	vthrow();	break;
      case FEED:	vfeed();	break;
      case FIND:
      case INVENTORY:	vfind();	break;
      case FILL:	vfill();	break;
      case READ:	vread();	break;
      case BLAST:	vblast();	break;
      case BREAK:	vbreak();	break;
      case WAKE:	vwake();	break;
      default:
	printf("This verb is not implemented yet.\n");
  }
}

/*
  CARRY TAKE etc.
*/
void vtake()
{
  int msg, i;

  if (toting(object)) {
	actspk(verb);
	return;
  }

  /* Special case objects and fixed objects */
  msg = 25;
  if (object == PLANT && g.prop[PLANT] <= 0) msg = 115;
  if (object == BEAR && g.prop[BEAR] == 1) msg = 169;
  if (object == CHAIN && g.prop[BEAR] != 0) msg = 170;
  if (g.fixed[object]) {
	rspeak(msg);
	return;
  }

  /* Special case for liquids */
  if (object == WATER || object == OIL) {
	if (!here(BOTTLE) || liq() != object) {
		object = BOTTLE;
		if (toting(BOTTLE) && g.prop[BOTTLE] == 1) {
			vfill();
			return;
		}
		if (g.prop[BOTTLE] != 1) msg = 105;
		if (!toting(BOTTLE)) msg = 104;
		rspeak(msg);
		return;
	}
	object = BOTTLE;
  }
  if (g.holding >= 7) {
	rspeak(92);
	return;
  }

  /* Special case for bird. */
  if (object == BIRD && g.prop[BIRD] == 0) {
	if (toting(ROD)) {
		rspeak(26);
		return;
	}
	if (!toting(CAGE)) {
		rspeak(27);
		return;
	}
	g.prop[BIRD] = 1;
  }
  if ((object == BIRD || object == CAGE) &&
      g.prop[BIRD] != 0)
	carry((BIRD + CAGE) - object, g.loc);
  carry(object, g.loc);
  /* Handle liquid in bottle */
  i = liq();
  if (object == BOTTLE && i != 0) g.place[i] = -1;
  rspeak(54);
}

/*
  DROP etc.
*/
void vdrop()
{
  int i;

  /* Check for dynamite */
  if (toting(ROD2) && object == ROD && !toting(ROD)) object = ROD2;
  if (!toting(object)) {
	actspk(verb);
	return;
  }

  /* Snake and bird */
  if (object == BIRD && here(SNAKE)) {
	rspeak(30);
	if (g.closed) dwarfend();
	dstroy(SNAKE);
	g.prop[SNAKE] = -1;
  }

  /* Coins and vending machine */
  else if (object == COINS && here(VEND)) {
	dstroy(COINS);
	drop(BATTERIES, g.loc);
	pspeak(BATTERIES, 0);
	return;
  }

  /* Bird and dragon (ouch!!) */
  else if (object == BIRD && at(DRAGON) && g.prop[DRAGON] == 0) {
	rspeak(154);
	dstroy(BIRD);
	g.prop[BIRD] = 0;
	if (g.place[SNAKE] != 0) ++g.tally2;
	return;
  }

  /* Bear and troll */
  if (object == BEAR && at(TROLL)) {
	rspeak(163);
	move(TROLL, 0);
	move((TROLL + MAXOBJ), 0);
	move(TROLL2, 117);
	move((TROLL2 + MAXOBJ), 122);
	juggle(CHASM);
	g.prop[TROLL] = 2;
  }

  /* Vase */
  else if (object == VASE) {
	if (g.loc == 96)
		rspeak(54);
	else {
		g.prop[VASE] = at(PILLOW) ? 0 : 2;
		pspeak(VASE, g.prop[VASE] + 1);
		if (g.prop[VASE] != 0) g.fixed[VASE] = -1;
	}
  }

  /* Handle liquid and bottle */
  i = liq();
  if (i == object) object = BOTTLE;
  if (object == BOTTLE && i != 0) g.place[i] = 0;
  /* Handle bird and cage */
  if (object == CAGE && g.prop[BIRD] != 0) drop(BIRD, g.loc);
  if (object == BIRD) g.prop[BIRD] = 0;
  drop(object, g.loc);
}

/*
  LOCK, UNLOCK, OPEN, CLOSE etc.
*/
void vopen()
{
  int msg, oyclam;

  switch (object) {
      case CLAM:
      case OYSTER:
	oyclam = (object == OYSTER ? 1 : 0);
	if (verb == LOCK)
		msg = 61;
	else if (!toting(TRIDENT))
		msg = 122 + oyclam;
	else if (toting(object))
		msg = 120 + oyclam;
	else {
		msg = 124 + oyclam;
		dstroy(CLAM);
		drop(OYSTER, g.loc);
		drop(PEARL, 105);
	}
	break;
      case DOOR:
	msg = (g.prop[DOOR] == 1 ? 54 : 111);
	break;
      case CAGE:	msg = 32;	break;
      case KEYS:	msg = 55;	break;
      case CHAIN:
	if (!here(KEYS))
		msg = 31;
	else if (verb == LOCK) {
		if (g.prop[CHAIN] != 0)
			msg = 34;
		else if (g.loc != 130)
			msg = 173;
		else {
			g.prop[CHAIN] = 2;
			if (toting(CHAIN)) drop(CHAIN, g.loc);
			g.fixed[CHAIN] = -1;
			msg = 172;
		}
	} else {
		if (g.prop[BEAR] == 0)
			msg = 41;
		else if (g.prop[CHAIN] == 0)
			msg = 37;
		else {
			g.prop[CHAIN] = 0;
			g.fixed[CHAIN] = 0;
			if (g.prop[BEAR] != 3) g.prop[BEAR] = 2;
			g.fixed[BEAR] = 2 - g.prop[BEAR];
			msg = 171;
		}
	}
	break;
      case GRATE:
	if (!here(KEYS))
		msg = 31;
	else if (g.closing) {
		if (!g.panic) {
			g.clock2 = 15;
			++g.panic;
		}
		msg = 130;
	} else {
		msg = 34 + g.prop[GRATE];
		g.prop[GRATE] = (verb == LOCK ? 0 : 1);
		msg += 2 * g.prop[GRATE];
	}
	break;
      default:	msg = 33;
}
  rspeak(msg);
}

/*
  SAY etc.
*/
void vsay()
{
  int wtype, wval;

  analyze(word1, &wtype, &wval);
  printf("Okay.\n%s\n", wval == SAY ? word2 : word1);
}

/*
  ON etc.
*/
void von()
{
  if (!here(LAMP))
	actspk(verb);
  else if (g.limit < 0)
	rspeak(184);
  else {
	g.prop[LAMP] = 1;
	rspeak(39);
	if (g.wzdark) {
		g.wzdark = 0;
		describe();
	}
  }
}

/*
  OFF etc.
*/
void voff()
{
  if (!here(LAMP))
	actspk(verb);
  else {
	g.prop[LAMP] = 0;
	rspeak(40);
  }
}

/*
  WAVE etc.
*/
void vwave()
{
  if (!toting(object) &&
      (object != ROD || !toting(ROD2)))
	rspeak(29);
  else if (object != ROD || !at(FISSURE) ||
	 !toting(object) || g.closing)
	actspk(verb);
  else {
	g.prop[FISSURE] = 1 - g.prop[FISSURE];
	pspeak(FISSURE, 2 - g.prop[FISSURE]);
  }
}

/*
  ATTACK, KILL etc.
*/
void vkill()
{
  int msg, i;

  switch (object) {
      case BIRD:
	if (g.closed)
		msg = 137;
	else {
		dstroy(BIRD);
		g.prop[BIRD] = 0;
		if (g.place[SNAKE] == 19) ++g.tally2;
		msg = 45;
	}
	break;
      case 0:	msg = 44;	break;
      case CLAM:
      case OYSTER:	msg = 150;	break;
      case SNAKE:	msg = 46;	break;
      case DWARF:
	if (g.closed) dwarfend();
	msg = 49;
	break;
      case TROLL:	msg = 157;	break;
      case BEAR:
	msg = 165 + (g.prop[BEAR] + 1) / 2;
	break;
      case DRAGON:
	if (g.prop[DRAGON] != 0) {
		msg = 167;
		break;
	}
	if (!yes(49, 0, 0)) break;
	pspeak(DRAGON, 1);
	g.prop[DRAGON] = 2;
	g.prop[RUG] = 0;
	move((DRAGON + MAXOBJ), -1);
	move((RUG + MAXOBJ), 0);
	move(DRAGON, 120);
	move(RUG, 120);
	for (i = 1; i < MAXOBJ; ++i)
		if (g.place[i] == 119 || g.place[i] == 121) move(i, 120);
	g.newloc = 120;
	return;
      default:
	actspk(verb);
	return;
  }
  rspeak(msg);
}

/*
  POUR
*/
void vpour()
{
  if (object == BOTTLE || object == 0) object = liq();
  if (object == 0) {
	needobj();
	return;
  }
  if (!toting(object)) {
	actspk(verb);
	return;
  }
  if (object != OIL && object != WATER) {
	rspeak(78);
	return;
  }
  g.prop[BOTTLE] = 1;
  g.place[object] = 0;
  if (at(PLANT)) {
	if (object != WATER)
		rspeak(112);
	else {
		pspeak(PLANT, g.prop[PLANT] + 1);
		g.prop[PLANT] = (g.prop[PLANT] + 2) % 6;
		g.prop[PLANT2] = g.prop[PLANT] / 2;
		describe();
	}
  } else if (at(DOOR)) {
	g.prop[DOOR] = (object == OIL ? 1 : 0);
	rspeak(113 + g.prop[DOOR]);
  } else
	rspeak(77);
}

/*
  EAT
*/
void veat()
{
  int msg;

  switch (object) {
      case FOOD:
	dstroy(FOOD);
	msg = 72;
	break;
      case BIRD:
      case SNAKE:
      case CLAM:
      case OYSTER:
      case DWARF:
      case DRAGON:
      case TROLL:
      case BEAR:	msg = 71;	break;
      default:
	actspk(verb);
	return;
  }
  rspeak(msg);
}

/*
  DRINK
*/
void vdrink()
{
  if (object != WATER)
	rspeak(110);
  else if (liq() != WATER || !here(BOTTLE))
	actspk(verb);
  else {
	g.prop[BOTTLE] = 1;
	g.place[WATER] = 0;
	rspeak(74);
  }
}

/*
  THROW etc.
*/
void vthrow()
{
  int msg, i;

  if (toting(ROD2) && object == ROD && !toting(ROD)) object = ROD2;
  if (!toting(object)) {
	actspk(verb);
	return;
  }

  /* Treasure to troll */
  if (at(TROLL) && object >= 50 && object < MAXOBJ) {
	rspeak(159);
	drop(object, 0);
	move(TROLL, 0);
	move((TROLL + MAXOBJ), 0);
	drop(TROLL2, 117);
	drop((TROLL2 + MAXOBJ), 122);
	juggle(CHASM);
	return;
  }

  /* Feed the bears... */
  if (object == FOOD && here(BEAR)) {
	object = BEAR;
	vfeed();
	return;
  }

  /* If not axe, same as drop... */
  if (object != AXE) {
	vdrop();
	return;
  }

  /* AXE is THROWN */
  /* At a dwarf... */
  if (i = dcheck()) {
	msg = 48;
	if (pct(75)) {
		g.dseen[i] = g.dloc[i] = 0;
		msg = 47;
		++g.dkill;
		if (g.dkill == 1) msg = 149;
	}
  }

  /* At a dragon... */
  else if (at(DRAGON) && g.prop[DRAGON] == 0)
	msg = 152;
  /* At the troll... */
  else if (at(TROLL))
	msg = 158;
  /* At the bear... */
  else if (here(BEAR) && g.prop[BEAR] == 0) {
	rspeak(164);
	drop(AXE, g.loc);
	g.fixed[AXE] = -1;
	g.prop[AXE] = 1;
	juggle(BEAR);
	return;
  }

  /* Otherwise it is an attack */
  else {
	verb = KILL;
	object = 0;
	itverb();
	return;
  }

  /* Handle the left over axe... */
  rspeak(msg);
  drop(AXE, g.loc);
  describe();
}

/*
  INVENTORY, FIND etc.
*/
void vfind()
{
  int msg;
  if (toting(object))
	msg = 24;
  else if (g.closed)
	msg = 138;
  else if (dcheck() && g.dflag >= 2 && object == DWARF)
	msg = 94;
  else if (at(object) ||
	 (liq() == object && here(BOTTLE)) ||
	 object == liqloc(g.loc))
	msg = 94;
  else {
	actspk(verb);
	return;
  }
  rspeak(msg);
}

/*
  FILL
*/
void vfill()
{
  int msg;
  int i;

  switch (object) {
      case BOTTLE:
	if (liq() != 0)
		msg = 105;
	else if (liqloc(g.loc) == 0)
		msg = 106;
	else {
		g.prop[BOTTLE] = g.cond[g.loc] & WATOIL;
		i = liq();
		if (toting(BOTTLE)) g.place[i] = -1;
		msg = (i == OIL ? 108 : 107);
	}
	break;
      case VASE:
	if (liqloc(g.loc) == 0) {
		msg = 144;
		break;
	}
	if (!toting(VASE)) {
		msg = 29;
		break;
	}
	rspeak(145);
	vdrop();
	return;
      default:	msg = 29;
}
  rspeak(msg);
}

/*
  FEED
*/
void vfeed()
{
  int msg;

  switch (object) {
      case BIRD:	msg = 100;	break;
      case DWARF:
	if (!here(FOOD)) {
		actspk(verb);
		return;
	}
	++g.dflag;
	msg = 103;
	break;
      case BEAR:
	if (!here(FOOD)) {
		if (g.prop[BEAR] == 0)
			msg = 102;
		else if (g.prop[BEAR] == 3)
			msg = 110;
		else {
			actspk(verb);
			return;
		}
		break;
	}
	dstroy(FOOD);
	g.prop[BEAR] = 1;
	g.fixed[AXE] = 0;
	g.prop[AXE] = 0;
	msg = 168;
	break;
      case DRAGON:
	msg = (g.prop[DRAGON] != 0 ? 110 : 102);
	break;
      case TROLL:	msg = 182;	break;
      case SNAKE:
	if (g.closed || !here(BIRD)) {
		msg = 102;
		break;
	}
	msg = 101;
	dstroy(BIRD);
	g.prop[BIRD] = 0;
	++g.tally2;
	break;
      default:	msg = 14;
}
  rspeak(msg);
}

/*
  READ etc.
*/
void vread()
{
  int msg;

  msg = 0;
  if (dark()) {
	printf("I see no %s here.\n", probj(object));
	return;
  }
  switch (object) {
      case MAGAZINE:	msg = 190;	break;
      case TABLET:	msg = 196;	break;
      case MESSAGE:	msg = 191;	break;
      case OYSTER:
	if (!toting(OYSTER) || !g.closed) break;
	yes(192, 193, 54);
	return;
      default:	;
}
  if (msg)
	rspeak(msg);
  else
	actspk(verb);
}

/*
  BLAST etc.
*/
void vblast()
{
  if (g.prop[ROD2] < 0 || !g.closed)
	actspk(verb);
  else {
	g.bonus = 133;
	if (g.loc == 115) g.bonus = 134;
	if (here(ROD2)) g.bonus = 135;
	rspeak(g.bonus);
	normend();
  }
}

/*
  BREAK etc.
*/
void vbreak()
{
  int msg;
  if (object == MIRROR) {
	msg = 148;
	if (g.closed) {
		rspeak(197);
		dwarfend();
	}
  } else if (object == VASE && g.prop[VASE] == 0) {
	msg = 198;
	if (toting(VASE)) drop(VASE, g.loc);
	g.prop[VASE] = 2;
	g.fixed[VASE] = -1;
  } else {
	actspk(verb);
	return;
  }
  rspeak(msg);
}

/*
  WAKE etc.
*/
void vwake()
{
  if (object != DWARF || !g.closed)
	actspk(verb);
  else {
	rspeak(199);
	dwarfend();
  }
}

/*
  Routine to speak default verb message
*/
void actspk(verb)
int verb;
{
  char i;

  if (verb < 1 || verb > 31) bug(39);
  i = actmsg[verb];
  if (i) rspeak(i);
}

/*
  Routine to indicate no reasonable
  object for verb found.  Used mostly by
  intransitive verbs.
*/
void needobj()
{
  int wtype, wval;

  analyze(word1, &wtype, &wval);
  printf("%s what?\n", wtype == 2 ? word1 : word2);
}
