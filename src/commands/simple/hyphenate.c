/* Hyphenate English text for mroff	Author: Alan I. Holub */

/* This filter reads English text and ouputs it with \% at reasonable
 * places to hyphenate words.  It is normally used as an mroff preprocessor.
 * The original algorithm is due to Donald Knuth.  The program itself
 * came with Michael Haardt's mroff package.
 */

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

typedef enum { FALSE, TRUE} bool;

#define HYPHEN          0x80
#define HYPHENATE(c)    ( (c) |=  HYPHEN )
#define UNHYPHENATE(c)  ( (c) &= ~HYPHEN )
#define HAS_HYPHEN(c)   ( (c) &   HYPHEN )

char **States;
extern char *Suffixes[];
extern char *Prefixes[];

#define ER(p,end) ((* p & 0x7f)=='e' && (*(p+1) & 0x7f) == 'r' && (p+1) == end)

#define CH ('z' + 1 )		/* {   0x7b  \173    */
#define GH ('z' + 2 )		/* |   0x7c  \174    */
#define PH ('z' + 3 )		/* }   0x7d  \175    */
#define SH ('z' + 4 )		/* ~   0x7e  \176    */
#define TH ('z' + 5 )		/* DEL   0x7f  \177    */

/* a b c d e f g h i j k l m n o p q r s t u v w x y z */
char vt[] = {1,0,0,0,1,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,1,0};

#define isvowel(c)  ( islower((c) & 0x7f)  &&  vt[ ((c) & 0x7f) - 'a'] )
#define isconsonant(c)  ((c) && !isvowel(c))

typedef struct
{
  char arg, type;
  void *variable;
} ARG;


_PROTOTYPE(char *suffix, (char *beg, char *end ));
_PROTOTYPE(char *prefix, (char *beg, char *end ));
_PROTOTYPE(char nextch, (char **pp, char *endp ));
_PROTOTYPE(int isweird, (int x, int y, char *p ));
_PROTOTYPE(void consonants, (char *beg, char *end ));
_PROTOTYPE(int hyphen, (char *beg, char *end ));
_PROTOTYPE(int next, (int cur_state, int cur_char ));
_PROTOTYPE(bool process_word, (char *w ));
_PROTOTYPE(int wcmp, (const void *word2, const void *tabp1 ));
_PROTOTYPE(int exception, (char *word, char *end ));
_PROTOTYPE(char *setarg, (ARG *argp, char *linep ));
_PROTOTYPE(ARG *findarg, (int c, ARG *tabp, int tabsize ));
_PROTOTYPE(int argparse, (int argc, char **argv, ARG *tabp, int tabsize ));
_PROTOTYPE(void fail, (char *word ));
_PROTOTYPE(void outtext, (int c ));
_PROTOTYPE(void outspec, (int c ));
_PROTOTYPE(int nextchar, (void));
_PROTOTYPE(int escapeseq, (void));
_PROTOTYPE(void nonletter, (void));
_PROTOTYPE(int special, (void));
_PROTOTYPE(int letter, (void));
_PROTOTYPE(void word1, (void));
_PROTOTYPE(void words, (void));
_PROTOTYPE(int macro, (void));
_PROTOTYPE(void line, (void));
_PROTOTYPE(void text, (FILE *fd ));
_PROTOTYPE(int main, (int argc, char *argv []));


char *suffix(beg, end)
char *beg, *end;
{
  register char *p, c, c2;
  register int state;
  register unsigned int times;

  state = 1;
  times = 0;
  States = Suffixes;

  for (p = end; p >= beg;) {
	state = next(state, c = *p-- & 0x7f);

	if (!('a' <= c && c <= 'z'))	/* Worter mit Grossbuch-  */
		return end;	/* staben werden nicht   */

	if (state == 0) {
		if (times == 0)
			p = end;	/* entferne endendes e,d */
		else if (times == 1)	/* entf. endendes ed, de */
			p = end - 1;	/* ee oder dd            */
		else
			return end;

		times++;
	}
	switch (state) {
	    case 86:
		c = *p & 0x7f;
		c2 = *(p - 1) & 0x7f;

		if (p - beg < 3) return end;	/* FAIL */
		else if ((c == c2) && (!isvowel(c))
			 && (c != 'f') && (c != 's')
			 && (c != 'l') && (c != 'z')) {
			--p;
		} else if (c == 'l' && strchr("bcdfghkptz", c2)) {
			p -= (c2 == 'k' && (*(p - 2) & 0x7f) == 'c')
				? 1 : 2;
		}

	    case 87:
		HYPHENATE(*(p + 1));
		return p;

	    case 88:
		HYPHENATE(*(p + 2));
		return(p + 1);

	    case 89:
		HYPHENATE(*(p + 5));
		HYPHENATE(*(p + 1));
		return p;

	    case 82:
		p++;		/* p += 3 */
	    case 81:
		p++;		/* p += 2 */
	    case 80:
		p++;		/* P += 1 */
	    case 83:
		end = p;
		HYPHENATE(*(p + 1));
		state = 1;
		break;

	    case 84:		/* Fehl-Status */
		return end;
	}
  }

  return end;
}
char *prefix(beg, end)
char *beg, *end;
{

  register char *p, c;
  register int state;

  state = 1;
  States = Prefixes;

  for (p = beg; p < end;) {
	switch (state = next(state, c = *p++ & 0x7f)) {
	    case 82:
		HYPHENATE(*p);
		HYPHENATE(*(p - 1));
		return p;
	    case 83:
		HYPHENATE(*p);
		HYPHENATE(*(p - 2));
		return p;
	    case 84:
		HYPHENATE(*p);
		HYPHENATE(*(p - 3));
		return p;

	    case 81:
		--p;
	    case 87:
		HYPHENATE(*p);
		return p;

	    case 85:
		--p;
	    case 86:
		beg = p;
		state = 1;
	    case 69:	HYPHENATE(*p);	break;

	    case 70:	HYPHENATE(*(p - 1));	break;
	    case 71:
	    case 72:
	    case 73:
	    case 74:
	    case 75:
	    case 76:
	    case 77:
	    case 78:	case 79:	break;
	}
  }

  return beg;
}

char *vccv_except[] =
{
  /* A  */ "",
  /* B  */ "lr",
  /* C  */ "lr",
  /* D  */ "gr",
  /* E  */ "",
  /* F  */ "lr",
  /* G  */ "lr",
  /* H  */ "",
  /* I  */ "",
  /* J  */ "",
  /* K  */ "n",
  /* L  */ "kq",
  /* M  */ "",
  /* N  */ "{kx",		/* CH, k, x */
  /* O  */ "",
  /* P  */ "lr",
  /* Q  */ "",
  /* R  */ "k",
  /* S  */ "pq",
  /* T  */ "{r",		/* CH, r    */
  /* U  */ "",
  /* V  */ "",
  /* W  */ "hlnr",
  /* X  */ "",
  /* Y  */ "",
  /* Z  */ "",
  /* CH */ "lr",
  /* GH */ "t",
  /* PH */ "r",
  /* SH */ "",
  /* TH */ "r"
};
char nextch(pp, endp)
char **pp, *endp;
{

  register char rval, *p;

  if ((p = *pp) > endp) return(char) 0;

  rval = *p++ & 0x7f;

  if ((*p & 0x7f) == 'h') {
	switch ((int) rval) {
	    case 't':
		rval = TH;
		p++;
		break;
	    case 's':
		rval = SH;
		p++;
		break;
	    case 'p':
		rval = PH;
		p++;
		break;
	    case 'c':
		rval = CH;
		p++;
		break;
	    case 'g':
		rval = GH;
		p++;
		break;
	}
  }
  *pp = p;
  return rval;
}
int isweird(x, y, p)
char x, y, *p;
{

  register unsigned int c1, c2, c3;

  c1 = *p++ & 0x7f;
  c2 = *p++ & 0x7f;
  c3 = *p & 0x7f;

  x &= 0x7f;
  y &= 0x7f;

  return(
	(
	 (c1 == 'e' && c2 == 'r')
	 || (c1 == 'a' && c2 == 'g' && c3 == 'e')
	 || (c1 == 'e' && c2 == 's' && c3 == 't')
	 )
	&&
	((x == 'f' && y == 't')
	 || (x == 'l' && y == 'd')
	 || (x == 'm' && y == 'p')
	 || (x == 's' && y == 't')
	 || (x == 'n' && strchr("dgst", (int) (y & 0x7f)))
	 || (x == 'r' && strchr("gmnt", (int) (y & 0x7f)))
	 )
	);
}
void consonants(beg, end)
char *beg, *end;
{

  register char c1, c2, *cp;
  register char *p;

  while (1) {
state1:

	do {
		c2 = nextch(&beg, end);
	} while (isconsonant(c2));

	do {
		for (c1 = c2; isvowel(c1);) {
			cp = beg;
			c1 = nextch(&beg, end);
		}


		if (c1 == 'q' && *beg == 'u') {	/* Vqu */
			HYPHENATE(*cp);	/* V-qu */
			nextch(&beg, end);	/* uberspringe u */
			goto state1;
		}
		cp = beg;
		c2 = nextch(&beg, end);

	} while (isvowel(c2));

	if (!c1 || !c2) break;

	if (c1 == 'c' && c2 == 'k') {	/* Vck  */
		if (*beg) HYPHENATE(*beg);	/* Vck- */
	} else if (c1 == c2) {	/* ller(s) */
		if ((c1 != 'l' && c1 != 's') ||
		    (isvowel(*beg) && !ER(beg, end)))
			HYPHENATE(*cp);
	} else if (isvowel(*beg)) {	/* VCCV */
		if (!isweird(c1, c2, beg)) {
			if (!((p = vccv_except[(int) c1 - 'a'])
			      && strchr(p, (int) c2)))
				HYPHENATE(*cp);
		}
	}
  }

  UNHYPHENATE(*end);
}

int hyphen(beg, end)
char *beg, *end;
{

  register char *prefixp, *suffixp;
  int c;

  if (end - beg <= 4) return 0;

  for (prefixp = beg; prefixp <= end; prefixp++) {
	if (HAS_HYPHEN(*prefixp)) return 1;

	if (!islower(*prefixp)) return 0;
  }

  if (exception(beg, end)) return 1;

  suffixp = suffix(beg, end);	/* Trenne und entferne alle     */
  /* Endungen (Suffixe)           */
  if (suffixp == end) {
	c = *end & 0x7f;

	if (c == 's' || c == 'e') suffixp = end - 1;

	else if ((*(end - 1) & 0x7f) == 'e' && c == 'd')
		suffixp = end - 2;
  }
  prefixp = prefix(beg, suffixp);	/* Trenne und entferne alle */
  /* Vorsilben (Prafixe)      */

  if ((suffixp - prefixp) >= 3) {
	/* Die Anwendung der Konsonantenpaar-Regel findet nur auf
	 * Worter mit mindestens 4 Buchsten statt (nachdem Vor- und
	 * Nachsilben entfernt wurden) */

	consonants(prefixp, suffixp);
  }
  return 1;
}
int next(cur_state, cur_char)
char cur_char;
int cur_state;
{
  char *p = States[cur_state];
  int rval, i, c;

  c = cur_char & 0x7f;


  if (!*p)
	rval = (int) (p[(c - 'a') + 1]);
  else {
	for (rval = 0, i = *p++; --i >= 0; p += 2)
		if (c == p[0]) {
			rval = p[1];
			break;
		}
  }


  return(rval);
}

#define S(x)  char x[]

S(s0) =
{0,84,84,84,1,1,84,84,84,84,84,84,84,84,84,84,84,84,84,1,84,84,84,84,84,84,84};

S(s1) = {0,0,0,2,0,7,0,21,0,0,0,0,23,0,30,0,0,0,33,38,45,0,0,0,0,49,0};
S(s2) ={  1, 'i', 3};
S(s3) ={  1, 'p', 4};
S(s4) ={  1, 'o', 5};
S(s5) ={  1, 'c', 6};
S(s6) ={  1, 's', 87};
S(s7) ={0,0,0,0,0,0,0,0,0,0,0,0,8,0,0,0,4,0,10,0,16,0,15,0,0,0,19};
S(s8) ={  1, 'b', 9};
S(s9) ={  1, 'a', 55};
S(s10) ={  2, 'e', 11, 'u', 14};
S(s11) ={  1, 'h', 12};
S(s12) ={  1, 'p', 6};
S(s13) ={  2, 'n', 81, 'r', 81};
S(s14) ={  1, 't', 87};
S(s15) ={  1, 'i', 14};
S(s16) ={  1, 'a', 17};
S(s17) ={  2, 'c', 18, 'l', 18};
S(s18) ={0,88,0,0,0,88,0,0,0,88,0,0,0,0,0,88,0,0,0,0,0,88,0,0,0,88,0};
S(s19) ={  1, 'i', 20};
S(s20) ={  1, 'l', 80};
S(s21) ={  1, 'n', 22};
S(s22) ={  1, 'i', 86};
S(s23) ={  2, 'a', 25, 'u', 24};
S(s24) ={  1, 'f', 83};
S(s25) ={0,0,0,87,0,0,0,0,0,26,0,0,0,0,27,0,0,0,0,0,87,0,0,0,0,0,0};
S(s26) ={  2, 't', 87, 'c', 87};
S(s27) ={  1, 'o', 28};
S(s28) ={  1, 'i', 29};
S(s29) ={  1, 't', 89};
S(s30) ={  1, 'o', 31};
S(s31) ={  1, 'i', 32};
S(s32) ={  2, 't', 87, 'c', 6};
S(s33) ={  1, 'e', 34};
S(s34) ={  1, 'h', 35};
S(s35) ={  1, 'p', 36};
S(s36) ={  1, 'a', 37};
S(s37) ={  1, 'r', 87};
S(s38) ={  2, 's', 39, 'u', 41};
S(s39) ={  1, 'e', 40};
S(s40) ={  2, 'l', 83, 'n', 83};
S(s41) ={  1, 'o', 42};
S(s42) ={  1, 'i', 43};
S(s43) ={  1, 'c', 44};
S(s44)={0,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,88,87,
							88,88,88,88,88,88,88};
S(s45) ={  1, 'n', 46};
S(s46) ={  1, 'e', 47};
S(s47)={0,0,0,0,87,0,0,0,0,48,0,0,0,83,0,0,0,0,0,0,0,0,0,0,0,0,0};
S(s48) ={  1, 'c', 87};
S(s49)={0,0,0,0,0,0,0,50,35,0,0,0,83,0,0,0,0,0,51,0,0,0,0,0,0,0,0};
S(s50) ={  1, 'o', 87};
S(s51) ={  1, 'a', 52};
S(s52) ={  1, 'n', 53};
S(s53)={0,88,88,88,88,81,88,88,88,88,88,88,88,88,88,54,88,88,
						88,88,88,88,88,88,88,88,88};
S(s54) ={  1, 'i', 82};
S(s55)={0,0,0,0,0,80,0,0,80,80,0,80,80,0,0,80,0,0,0,0,13,80,80,80,80,80,0};


char *Suffixes[] =
{
 s0, s1, s2, s3, s4, s5, s6, s7, s8, s9,
 s10, s11, s12, s13, s14, s15, s16, s17, s18, s19,
 s20, s21, s22, s23, s24, s25, s26, s27, s28, s29,
 s30, s31, s32, s33, s34, s35, s36, s37, s38, s39,
 s40, s41, s42, s43, s44, s45, s46, s47, s48, s49,
 s50, s51, s52, s53, s54, s55
};
S(p0) =
{
  1, 0, 0
};

S(p1) =
{
  0, 0, 2, 4, 6, 9, 0, 0, 13, 22, 0, 0, 26, 29, 39, 41, 45, 50, 0, 53, 57, 64,
	0, 0, 0, 0, 0
};

S(p2) =
{
  1, 'e', 3
};

S(p3) =
{
  0, 0, 0, 81, 0, 0, 0, 0, 81, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 81, 0, 0, 0, 81, 0, 0, 0
};

S(p4) =
{
  1, 'o', 5
};

S(p5) =
{
  2, 'm', 87, 'n', 87
};

S(p6) =
{
  1, 'i', 7
};

S(p7) =
{
  1, 's', 8
};

S(p8) =
{
  0, 85, 85, 85, 85, 85, 85, 85, 0, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
	85, 85, 85, 85, 0, 85
};

S(p9) =
{
  2, 'q', 10, 'x', 87
};

S(p10) =
{
  1, 'u', 11
};

S(p11) =
{
  1, 'i', 12
};

S(p12) =
{
  0, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81, 81,
	81, 0, 81, 81, 81, 81
};

S(p13) =
{
  0, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0, 19, 0
};

S(p14) =
{
  1, 'n', 15
};

S(p15) =
{
  1, 'd', 87
};

S(p16) =
{
  1, 'r', 17
};

S(p17) =
{
  1, 's', 18
};

S(p18) =
{
  1, 'e', 87
};

S(p19) =
{
  1, 'p', 20
};

S(p20) =
{
  1, 'e', 21
};

S(p21) =
{
  1, 'r', 84
};

S(p22) =
{
  2, 'n', 69, 'm', 86
};

S(p23) =
{
  0, 81, 0, 0, 0, 0, 81, 81, 0, 0, 0, 0, 81, 81, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

S(p24) =
{
  2, 'e', 21, 'r', 25
};

S(p25) =
{
  1, 'o', 87
};

S(p26) =
{
  1, 'e', 27
};

S(p27) =
{
  1, 'x', 35
};

S(p28) =
{
  1, 'i', 87
};

S(p29) =
{
  0, 30, 0, 0, 0, 0, 0, 0, 0, 36, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 37, 0, 0, 0, 0, 0
};

S(p30) =
{
  0, 0, 0, 31, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 0, 0, 0, 35, 0, 0
};

S(p31) =
{
  1, 'r', 32
};

S(p32) =
{
  1, 'o', 83
};

S(p33) =
{
  1, 'h', 34
};

S(p34) =
{
  1, 'e', 82
};

S(p35) =
{
  1, 'i', 82
};

S(p36) =
{
  1, 'n', 35
};

S(p37) =
{
  1, 'l', 38
};

S(p38) =
{
  1, 't', 65
};

S(p39) =
{
  1, 'o', 40
};

S(p40) =
{
  1, 'n', 86
};

S(p41) =
{
  2, 'u', 42, 'v', 43
};

S(p42) =
{
  1, 't', 87
};

S(p43) =
{
  1, 'e', 44
};

S(p44) =
{
  1, 'r', 86
};

S(p45) =
{
  1, 's', 46
};

S(p46) =
{
  1, 'e', 47
};

S(p47) =
{
  1, 'u', 48
};

S(p48) =
{
  1, 'd', 49
};

S(p49) =
{
  1, 'o', 83
};

S(p50) =
{
  1, 'u', 51
};

S(p51) =
{
  1, 'a', 52
};

S(p52) =
{
  1, 'd', 87
};

S(p53) =
{
  0, 0, 0, 0, 0, 54, 0, 0, 0, 0, 0, 0, 0, 0, 0, 55, 0, 0, 0, 0, 0, 56, 0, 0, 0, 0, 0
};

S(p54) =
{
  1, 'm', 28
};

S(p55) =
{
  1, 'm', 18
};

S(p56) =
{
  2, 'b', 87, 'p', 20
};

S(p57) =
{
  2, 'h', 58, 'r', 61
};

S(p58) =
{
  1, 'e', 59
};

S(p59) =
{
  1, 'r', 60
};

S(p60) =
{
  1, 'e', 87
};

S(p61) =
{
  2, 'a', 62, 'i', 68
};

S(p62) =
{
  1, 'n', 63
};

S(p63) =
{
  1, 's', 23
};

S(p64) =
{
  1, 'n', 66
};

S(p65) =
{
  1, 'i', 83
};

S(p66) =
{
  0, 85, 85, 85, 70, 85, 85, 85, 85, 0, 85, 85, 85, 85, 85, 85, 85, 85, 85,
	85, 85, 85, 85, 85, 85, 85, 85
};

S(p67) =
{
  1, 'r', 87
};

S(p68) =
{
  0, 81, 0, 0, 0, 0, 81, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 81, 0, 0, 0, 0, 0
};

S(p69) =
{
  0, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85, 85,
	85, 24, 85, 85, 85, 85, 85, 85
};

S(p70) =
{
  1, 'e', 67
};

char *Prefixes[] =
{
 p0, p1, p2, p3, p4, p5, p6, p7, p8, p9,
 p10, p11, p12, p13, p14, p15, p16, p17, p18, p19,
 p20, p21, p22, p23, p24, p25, p26, p27, p28, p29,
 p30, p31, p32, p33, p34, p35, p36, p37, p38, p39,
 p40, p41, p42, p43, p44, p45, p46, p47, p48, p49,
 p50, p51, p52, p53, p54, p55, p56, p57, p58, p59,
 p60, p61, p62, p63, p64, p65, p66, p67, p68, p69,
 p70
};

bool process_word(w)
char *w;
{
  char word[80], *s = word;

  hyphen(w, w + strlen(w) - 1);
  strcpy(word, w);
  while (*s) {
	if (HAS_HYPHEN(*s)) {
		*w++ = '\\';
		*w++ = '%';
	}
	*w++ = UNHYPHENATE(*s++);
  }
  *w++ = '\0';
  return TRUE;
}


char *Hyexcept[] =
{
 "ab-sorb-ent", "ac-cept-able", "ac-ceptor",
 "ac-cord-ance", "ac-count-ant", "ac-know-ledge",
 "adapt-able", "adapt-er", "af-firm-ative",
 "al-go-rithm", "an-nouncer", "ant-acid",
 "ant-eater", "ant-hill", "an-tiq-uity",
 "any-thing", "apart-heid", "arch-an-gel",
 "arch-ery", "ar-mi-stice", "art-ist",
 "askance", "astig-ma-tism", "astir",
 "astonish-ment", "at-mos-phere", "bal-ding",
 "bar-on-ess", "beach-comber", "beck-on",
 "bes-tial", "be-tween", "bib-li-og-raphy",
 "bind-ery", "bi-no-mial", "blast-off",
 "board-er", "bomb-er", "bouncer",
 "bound-ary", "buff-er", "bull-ish",
 "buzz-er", "by-stand-er", "candle-stick",
 "carb-on", "cast-away", "cast-off",
 "cat-ion", "cav-ern-ous", "cen-ter",
 "change-over", "child-ish", "chordal",
 "civ-i-lize", "class-ify", "class-room",
 "climb-er", "clinch-er",
 "coars-en", "cognac",
 "cole-slaw", "com-a-tose", "com-bat-ive",
 "come-back", "co-me-dian", "com-men-da-tory",
 "comp-troller", "com-put-abil-ity", "con-de-scend",
 "cone-flower", "con-form-ity", "con-sult-ant",
 "con-test-ant", "con-trol-lable", "co-nun-drum",
 "con-vert-ible", "co-star", "count-ess",
 "court-house", "court-yard", "cre-scendo",
 "crest-fallen", "cross-over", "crypt-analysis",
 "crys-tal-lize", "curl-i-cue", "damp-en",
 "damp-est", "dar-ling", "debt-or",
 "dec-la-ra-tion", "de-cre-scendo", "de-duct-ible",
 "de-form-ity", "de-gree", "de-pend-able",
 "de-pend-ent", "de-scend-ent", "de-scent",
 "de-test-able", "di-gest-ible", "dis-cern-ible",
 "dis-miss-al", "dis-till-ery", "dump-ling",
 "earth-en-ware", "east-ern", "egg-head",
 "egg-nog", "eld-est", "else-where",
 "eq-uable", "equipped", "err-ing",
 "es-tab-lish", "eu-logy", "eve-ning",
 "every-thing", "ex-ac-ti-tude", "ex-ist-ence",
 "ex-pend-able", "ex-press-ible", "fall-out",
 "false-hood", "far-thing", "fencer",
 "fiend-ish", "for-eign-er", "fore-short-en",
 "fore-stall", "found-ling", "gen-er-ator",
 "gold-en", "handle-bar", "hang-out",
 "hang-over", "hap-hazard", "ha-rangue",
 "hard-en", "hard-ened", "hard-est",
 "harp-ist", "haz-ard-ous", "heart-ache",
 "heart-ily", "hence-forth", "her-bal",
 "hogs-head", "hold-out", "hold-over",
 "hold-up",
 "idler", "im-mo-bi-lize",
 "im-pass-able", "im-per-turb-able", "inch-worm",
 "in-clem-ent", "in-con-test-able", "in-de-pend-ent",
 "in-di-gest-ible", "ineq-uity", "in-ex-acti-tude",
 "in-ex-haust-ible", "in-form-ant", "iniq-uity",
 "ink-blot", "ink-ling", "inn-keeper",
 "in-sa-tiable", "in-te-rior", "in-ter-rupt-ible",
 "ir-re-vers-ible", "jeop-ard-ize", "kib-itzer",
 "land-owner", "launch-er", "left-ist",
 "left-over", "less-en", "life-style",
 "lift-off", "lime-stone", "li-on-ess",
 "liq-uefy", "liq-uid", "liq-ui-date",
 "liq-ui-da-tion", "liq-uor", "live-stock",
 "lull-aby", "lunch-eon", "lus-cious",
 "main-spring", "mast-head", "me-ringue",
 "me-ta-bo-lize", "met-al", "mile-stone",
 "mince-meat", "min-is-ter", "min-is-try",
 "mo-bi-lize", "mod-ern-ize", "mo-nop-o-lize",
 "morgue", "needle-work", "neg-li-gible",
 "ne-go-tiable", "nerv-ous", "nest-ling",
 "non-con-form-ist", "none-the-less", "non-ex-ist-ent",
 "non-metal", "north-east", "north-ern",
 "nurse-maid", "nurs-ery", "ob-serv-able",
 "ob-server", "off-beat", "off-hand",
 "off-print", "off-shoot", "off-shore",
 "off-spring", "orange-ade", "out-land-ish",
 "pal-ate", "pass-able", "ped-a-gogy",
 "pent-house", "per-cent-age", "pe-ri-odic",
 "per-sist-ent", "pet-al", "pho-to-stat",
 "play-thing", "pleb-i-scite", "plumb-er",
 "poly-no-mial", "port-hole", "post-al",
 "post-hu-mous", "pre-dict-able", "pre-req-ui-site",
 "pre-school", "pre-serv-ative", "pre-vious",
 "priest-hood", "prob-abil-ity", "prob-able",
 "pro-ce-dure", "pro-gram", "pro-gram-mer",
 "pro-grams", "psalm-ist", "pub-li-ca-tion",
 "pub-lish", "qua-drille", "ranch-er",
 "rattle-snake", "re-corder", "re-hears-al",
 "rent-al", "re-place-ment", "rep-re-sentative",
 "req-ui-si-tion", "re-scind", "re-search-er",
 "re-solv-able", "re-spect-able", "re-start-ed",
 "re-state-ment", "re-store", "re-vers-ible",
 "re-volv-er", "roll-away", "round-about",
 "sap-ling", "sea-scape", "self-ish",
 "sell-out", "send-off", "sense-less",
 "serv-er", "serv-ice-able", "sharpen",
 "shoe-string", "short-en", "shy-ster",
 "sib-ling", "side-step", "side-swipe",
 "si-lencer",
 "smoke-stack",
 "snake-skin", "so-ciable", "soft-hearted",
 "solv-able", "som-er-sault", "some-thing",
 "sta-bi-lize", "stand-ard-ize", "stand-out",
 "star-ling", "stat-ure", "ster-ling",
 "stew-ard-ess", "stiff-en", "sub-se-quence",
 "sug-gest-ible", "su-pe-rior", "surf-er",
 "tan-ta-lize", "thermo-stat", "tongue",
 "torque", "toss-up", "trench-ant",
 "turn-about", "turn-over", "turn-table",
 "ubiq-ui-tous", "una-nim-ity", "u-nan-i-mous",
 "un-civ-i-lized", "un-class-ified", "un-con-trollable",
 "unc-tuous", "un-der-stand-able", "un-err-ing",
 "un-gov-ern-able", "un-pre-dict-able", "un-search-able",
 "un-so-ciable", "un-solv-able", "up-swing",
 "venge-ance", "vict-ual", "vignette",
 "volt-age", "wall-eye", "waste-bas-ket",
 "waste-land", "watt-meter", "weak-ling",
 "west-ern-ize", "when-ever", "whisk-er",
 "wors-en", "yard-age", "year-ling"
};

#define TABSIZE ( sizeof( Hyexcept ) / sizeof(char *) )

int wcmp(word2, tabp1)
_CONST void *word2;
_CONST void *tabp1;
{
  /* Zur Behandlung des Plurals modifiziert am 17.7.87. Die Funktion
   * liefert TRUE, falls das Wort passt, jedoch auf ein s oder es endet. */

  char *word = (char *) word2;
  char **tabp = (char **) tabp1;
  register char *p = *tabp;

  while (*word && *p) {
	if (*p == '-') p++;

	if (*word != *p) break;

	word++;
	p++;
  }

  if (!*p && (word[0] == 's' ||
	    (word[0] == 'e' && word[1] == 's')))
	return 0;

  return(*word - *p);
}

int exception(word, end)
char *word, *end;
{
  /* Durchsucht die Ausnahmeliste und liefert einen Zeiger auf den
   * Eintrag, falls es einen gibt, Null, falls nicht. */

  char **pp, *p;
  char oend;
  int rval = 0;

  oend = *++end;
  *end = 0;

  pp = (char **) bsearch(word, Hyexcept, TABSIZE, sizeof(char *),  wcmp);

  if (pp) {
	for (p = *pp; *p; word++, p++) {
		if (*p == '-') {
			HYPHENATE(*word);
			p++;
		}
	}

	rval = 1;
  }
  *end = oend;
  return rval;
}

char linebuf[512], *s;
bool font4 = FALSE, only_font4 = FALSE, usage = FALSE;

#define INT_ARG 0
#define BOOL_ARG 1
#define CHAR_ARG 2
#define STRING_ARG 3

char *setarg( argp, linep ) ARG *argp; char *linep;
/* set an argument.  argp points to the entry in the argument table, which
 * fits to *linep.  returns linep which points after the processed argument.
 */
{
  char *not_s;			/* too many variables named s, s1, ... */

  ++linep;

  switch( argp->type )
  {
    case INT_ARG:
	*(int*)argp->variable=atoi(linep);
	while (*linep) linep++;
	break;

    case BOOL_ARG: 
	*(int*)argp->variable = 1; break;

    case CHAR_ARG: 
	 *(char*)argp->variable = *linep++; break;

    case STRING_ARG:
	not_s = (char*) argp->variable;
	while (*linep) *not_s++ = *linep++;
	*not_s='\0';
	break;
  }
  return(linep);
}

ARG *findarg(c, tabp, tabsize) 
char c; ARG *tabp; 
int tabsize;
{
  for (; --tabsize >= 0 ; tabp++ )
  if (tabp->arg == c ) return tabp;

  return NULL;
}

int argparse(argc,argv,tabp,tabsize)
int argc; char **argv; 
ARG *tabp; 
int tabsize;
{
  int nargc;
  char **nargv, *p;
  ARG   *argp;

  nargc = 1 ;
  for(nargv = ++argv ; --argc > 0 ; argv++ )
  {
    if (**argv=='-' && (argp = findarg(*(p=argv[0]+1), tabp, tabsize)))
      do 
        if (*(p=setarg(argp,p))) argp=findarg(*p,tabp,tabsize);
      while (*p);
    else
    {
      *nargv++ = *argv ;
      nargc++;
    }
  }
  return nargc ;
}


void fail(word)
char *word;
{
  fprintf(stderr, "Can't process '%s'\nLine: %s\n", word, linebuf);
}

char transbuf[512], *t = transbuf;

void outtext(c)
char c;
{
  *t++ = c;
}

void outspec(c)
char c;
{
  if (t != transbuf) {
	*t++ = '\0';
	if (only_font4 && !font4)
		printf("%s", transbuf);
	else if (process_word(transbuf))
		printf("%s", transbuf);
	t = transbuf;
  }
  putchar(c);
}

int nextchar()
{
  if (*s == '\0') return 0;
  s++;
  return 1;
}

/* I don't echo the backslash, because special did it when it failed to match
   the special rule.  Not the best way, but who cares.
*/
int escapeseq()
{
  if (*s != '\\') return 0;
  nextchar();
  switch (*s) {
      case '*':
      case 'n':
	{
		outspec(*s);
		nextchar();
		if (*s == '(') {
			outspec('(');
			nextchar();
			outspec(*s);
			nextchar();
		}
		outspec(*s);
		nextchar();
		break;
	}
      default:
	outspec(*s);
	nextchar();
  }
  return 1;
}

void nonletter()
{
  if (!escapeseq()) {
	outspec(*s);
	nextchar();
  }
}

/* This rule is context-sensitive, because not all special characters are
 * accepted as a letter, see the return codes.
 */
int special()
{
  char *olds;

  if (*s != '\\') return 0;
  olds = s;
  nextchar();
  switch (*s) {
      case '%':
	outspec('\\');
	outspec('%');
	nextchar();
	return 1;
      case 'z':
	outspec('\\');
	outspec('z');
	nextchar();
	return 1;
      case 'f':			/* font ::= 'f' anychar */
	{
		outspec('\\');
		outspec('f');
		nextchar();
		font4 = (*s == '4');
		outspec(*s);
		nextchar();
		return 1;
	}

      case 's':			/* size ::= [ '+' | '-' ] { digit } */
	{
		outspec('\\');
		outspec('s');
		nextchar();
		if (*s == '-' || *s == '+') nextchar();
		while (*s >= '0' && *s <= '9') nextchar();
		return 1;
	}

      case '(':
	{
		outspec('\\');
		outspec('(');
		nextchar();
		outspec(*s);
		nextchar();
		return 2;
	}

      default:
	outspec('\\');
	s = olds;
	return 0;
  }
}

int letter()
{
  int x;

  x = special();
  if (x) return 1;
  if (x == 0)
	if ((*s >= 'a' && *s <= 'z') || (*s >= 'A' && *s <= 'Z')) {
		outtext(*s);
		nextchar();
		return 1;
	}
  return 0;
}

void word1()
{
  if (letter()) {
	while (letter());
  }
}

void words()
{
  while (*s) {
	word1();
	if (*s) nonletter();
  }
}

/* Arguments not implemented yet */
int macro()
{
  if (*s == '.') {
	printf("%s", s);
	return 1;
  } else
	return 0;
}

void line()
{
  if (!macro()) words();
}

void text(fd)
FILE *fd;
{
  int l;

  while (fgets(linebuf, sizeof(linebuf), fd)) {
	if ((l = strlen(linebuf)) != 0) linebuf[l - 1] = '\0';
	s = linebuf;
	line();
	outspec('\n');
  }
}

#define argnum 2

ARG argtab[argnum] =
{
 '?', BOOL_ARG, (void *) &usage,
 '4', BOOL_ARG, (void *) &only_font4
};

int main(argc, argv)
int argc;
char *argv[];
{
  FILE *in;

  argc = argparse(argc, argv, argtab, argnum);
  if (usage) {
	fprintf(stderr, "Usage: %s {-4} {file}\n", argv[0]);
	exit(0);
  }
  if (argc == 2) {
	if ((in = fopen(argv[1], "r")) == NULL) {
		fprintf(stderr, "%s: unvalid argument\n", argv[0]);
		exit(1);
	}
  } else
	in = stdin;
  text(in);
  if (in != stdin) fclose(in);
  return 0;
}
