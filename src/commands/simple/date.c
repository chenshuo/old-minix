
/* date - Display (or set) the date and time		Author: V. Archer */

#include <sys/types.h>
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <stdio.h>

#define	MIN	60L		/* # seconds in a minute */
#define	HOUR	(60 * MIN)	/* # seconds in an hour */
#define	DAY	(24 * HOUR)	/* # seconds in a day */
#define	YEAR	(365 * DAY)	/* # seconds in a (non-leap) year */

int uflag;

_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(void pstring, (char *s, int len));
_PROTOTYPE(void pdecimal, (int d, int digits));
_PROTOTYPE(void fmtdate, (FILE *fp, char *format, struct tm *p));
_PROTOTYPE(void set_time, (char *t));
_PROTOTYPE(void usage, (void));

/* Main module. Handles P1003.2 date and system administrator's date. The
 * date entered should be given GMT, regardless of the system's TZ!
 */
int main(argc, argv)
int argc;
char **argv;
{
  time_t t;
  char *format;
  char time_buf[40];

  argc--;
  argv++;
  if (argc) {
	if (**argv == '-' && argv[0][1] == 'q') {
		printf("\nPlease enter date: MMDDYYhhmmss. Then hit the RETURN key.\n");
		fgets(time_buf, sizeof time_buf, stdin);
		set_time(time_buf);
		argc--;
		argv++;
	} else if (isdigit(argv[0][1])) {
		set_time(argv[0]);
		argc--;
		argv++;
	}
  }
  if (argc && **argv == '-' && argv[0][1] == 'u') {
	uflag = 1;
	argc--;
	argv++;
  } else
	uflag = 0;

  if (argc > 1 || (argc && **argv != '+')) usage();

  if (argc)
	format = argv[0] + 1;
  else
	format = "%c";

  time(&t);
  fmtdate(stdout, format, uflag ? gmtime(&t) : localtime(&t));
  putchar('\n');
  fflush(stdout);
  return(0);
}

/* Internal function that prints a n-digits number. Replaces stdio in our
 * specific case.
 */
void pdecimal(d, digits)
int d, digits;
{
  digits--;
  if (d > 9 || digits > 0) pdecimal(d / 10, digits);
  putchar('0' + (d % 10));
}

/* Internal function that prints a fixed-size string. Replaces stdio in our
 * specific case.
 */
void pstring(s, len)
char *s;
int len;
{
  while (*s)
	if (len--)
		putchar(*s++);
	else
		break;
}

/* Format the date, using the given locale string. A special case is the
 * TZ which might be a sign followed by four digits (New format time zone).
 */
void fmtdate(fp, format, p)
FILE *fp;
char *format;
struct tm *p;
{
  int i;
  char *s;
  static char *wday[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
		       "Thursday", "Friday", "Saturday"};
  static char *month[] = {"January", "February", "March", "April",
			"May", "June", "July", "August",
		    "September", "October", "November", "December"};

  while (*format)
	if (*format == '%') {
		switch (*++format) {
		    case 'A':
			pstring(wday[p->tm_wday], -1);
			break;
		    case 'B':
			pstring(month[p->tm_mon], -1);
			break;
		    case 'D':
			pdecimal(p->tm_mon + 1, 2);
			putchar('/');
			pdecimal(p->tm_mday, 2);
			putchar('/');
		    case 'y':
			pdecimal(p->tm_year % 100, 2);
			break;
		    case 'H':
			pdecimal(p->tm_hour, 2);
			break;
		    case 'I':
			i = p->tm_hour % 12;
			pdecimal(i ? i : 12, 2);
			break;
		    case 'M':
			pdecimal(p->tm_min, 2);
			break;
		    case 'X':
		    case 'T':
			pdecimal(p->tm_hour, 2);
			putchar(':');
			pdecimal(p->tm_min, 2);
			putchar(':');
		    case 'S':
			pdecimal(p->tm_sec, 2);
			break;
		    case 'U':
			pdecimal((p->tm_yday - p->tm_wday + 13) / 7, 2);
			break;
		    case 'W':
			if (--(p->tm_wday) < 0) p->tm_wday = 6;
			pdecimal((p->tm_yday - p->tm_wday + 13) / 7, 2);
			if (++(p->tm_wday) > 6) p->tm_wday = 0;
			break;
		    case 'Y':
			pdecimal(p->tm_year + 1900, 4);
			break;
		    case 'Z':
			if (uflag)
				s = "GMT";	/* or "Z" as in X400 */
			else if (!(s = getenv("TZ")))
				s = "";
			if (*s == '+' || *s == '-')
				pstring(s, 5);
			else
				pstring(s, 3);
			break;
		    case 'a':
			pstring(wday[p->tm_wday], 3);
			break;
		    case 'b':
		    case 'h':
			pstring(month[p->tm_mon], 3);
			break;
		    case 'c':
			if (!(s = getenv("LC_TIME")))
				s = "%a %b %e %T %Z %Y";
			fmtdate(fp, s, p);
			break;
		    case 'd':
			pdecimal(p->tm_mday, 2);
			break;
		    case 'e':
			if (p->tm_mday < 10) putchar(' ');
			pdecimal(p->tm_mday, 1);
			break;
		    case 'j':
			pdecimal(p->tm_yday + 1, 3);
			break;
		    case 'm':
			pdecimal(p->tm_mon + 1, 2);
			break;
		    case 'n':	putchar('\n');	break;
		    case 'p':
			if (p->tm_hour < 12)
				putchar('A');
			else
				putchar('P');
			putchar('M');
			break;
		    case 'r':
			fmtdate(fp, "%I:%M:%S %p", p);
			break;
		    case 't':	putchar('\t');	break;
		    case 'w':
			putchar('0' + p->tm_wday);
			break;
		    case 'x':
			fmtdate(fp, "%B %e %Y", p);
			break;
		    case '%':	putchar('%');	break;
		    case '\0':	format--;
		}
		format++;
	} else
		putchar(*format++);
}

/* Set a new GMT time and maybe date. */
void set_time(t)
char *t;
{
  struct tm tm;				/* user specified time */
  time_t now;				/* current time */
  int leap;				/* current year is leap year */
  int i;				/* general index */
  int fld;				/* number of fields */
  int f[6];				/* time fields */
  static int days_per_month[2][12] = {
  { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
  { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }};

/* Get current time just in case */
  now = time((time_t *) 0);
  tm  = *localtime(&now);
  tm.tm_sec   = 0;
  tm.tm_mon++;
  tm.tm_year %= 100;

/* Parse the time */
#if '0'+1 != '1' || '1'+1 != '2' || '2'+1 != '3' || '3'+1 != '4' || \
    '4'+1 != '5' || '5'+1 != '6' || '6'+1 != '7' || '7'+1 != '8' || '8'+1 != '9'
  << Code unsuitable for character collating sequence >>
#endif

  for (fld = 0; fld < sizeof(f)/sizeof(f[0]); fld++) {
	if (*t == 0) break;
	f[fld] = 0;
	for (i = 0; i < 2; i++, t++) {
		if (*t < '0' || *t > '9') usage();
		f[fld] = f[fld] * 10 + *t - '0';
	}
  }

  switch (fld) {
  case 2:
	tm.tm_hour = f[0]; tm.tm_min  = f[1]; break;

  case 3:
	tm.tm_hour = f[0]; tm.tm_min  = f[1]; tm.tm_sec  = f[2];
	break;

  case 5:
  	tm.tm_mon  = f[0]; tm.tm_mday = f[1]; tm.tm_year = f[2];
	tm.tm_hour = f[3]; tm.tm_min  = f[4];
	break;

  case 6:
	tm.tm_mon  = f[0]; tm.tm_mday = f[1]; tm.tm_year = f[2];
	tm.tm_hour = f[3]; tm.tm_min  = f[4]; tm.tm_sec  = f[5];
	break;

  default:
	usage();
  }

/* Convert the time into seconds since 1 January 1970 */
  if (tm.tm_year < 70)
    tm.tm_year += 100;
  leap = (tm.tm_year % 4 == 0 && tm.tm_year % 400 != 0);
  if (tm.tm_mon  < 1  || tm.tm_mon  > 12 ||
      tm.tm_mday < 1  || tm.tm_mday > days_per_month[leap][tm.tm_mon-1] ||
      tm.tm_hour > 23 || tm.tm_min  > 59) {
    fputs("Illegal date format\n", stderr);
    exit(1);
  }

/* Convert the time into Minix time - zone independent code */
  {
    time_t utctime;			/* guess at unix time */
    time_t nextbit;			/* next bit to try */
    int rv;				/* result of try */
    struct tm *tmp;			/* local time conversion */

#define COMPARE(a,b)	((a) != (b)) ? ((a) - (b)) :

    utctime = 1;
    do {
      nextbit = utctime;
      utctime = nextbit << 1;
    } while (utctime >= 1);

    for (utctime = 0; ; nextbit >>= 1) {

      utctime |= nextbit;
      tmp = localtime(&utctime);
      if (tmp == 0) continue;

      rv = COMPARE(tmp->tm_year,    tm.tm_year)
           COMPARE(tmp->tm_mon + 1, tm.tm_mon)
	   COMPARE(tmp->tm_mday,    tm.tm_mday)
	   COMPARE(tmp->tm_hour,    tm.tm_hour)
	   COMPARE(tmp->tm_min,     tm.tm_min)
	   COMPARE(tmp->tm_sec,     tm.tm_sec)
	   0;

      if (rv > 0)
        utctime &= ~nextbit;
      else if (rv == 0)
        break;

      if (nextbit == 0) {
	uflag = 1;
        fputs("Inexact conversion to UTC from ", stderr);
        fmtdate(stderr, "%c\n", localtime(&utctime) );
	exit(1);
      }
    }
    if (stime(&utctime)) {
      fputs("No permission to set time\n", stderr);
      exit(1);
    }
  }
}

/* (Extended) Posix prototype of date. */
void usage()
{
  fputs("Usage: date [-q | [MMDDYY]hhmm[ss]] [-u] [+format]\n", stderr);
  exit(1);
}
