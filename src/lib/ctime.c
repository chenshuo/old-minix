struct tm  {
  int	tm_sec;
  int	tm_min;
  int	tm_hour;
  int	tm_mday;
  int	tm_mon;
  int	tm_year;
  int	tm_wday;
  int	tm_yday;
  int	tm_isdst;
};

struct timeb {
  long	time;
  unsigned short millitm;
  short	timezone;
  short	dstflag;
};

extern struct tm *localtime();
extern char *asctime();

char *ctime(clock)
	long *clock;
{
	return asctime(localtime(clock));
}

static int monthsize[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

#define SECS_DAY (24*60L*60L)
#define YEARSIZE(year)	((year) % 4 ? 365 : 366)

struct tm *
gmtime(clock)
	long *clock;
{
	long cl = *clock;
	long dayclock, dayno;
	static struct tm tm_buf;
	register struct tm *pbuf = &tm_buf;
	register int *months = monthsize;
	int year = 1970;

	dayclock = cl % SECS_DAY;
	dayno = cl / SECS_DAY;

	pbuf->tm_sec = dayclock % 60;
	pbuf->tm_min = (dayclock % 3600) / 60;
	pbuf->tm_hour = dayclock / 3600;
	pbuf->tm_wday = (dayno + 4) % 7; /* day 0 was a thursday */
	while (dayno >= YEARSIZE(year)) {
		dayno -= YEARSIZE(year);
		year++;
	}
	pbuf->tm_year = year - 1900;
	pbuf->tm_yday = dayno;
	pbuf->tm_isdst = 0;
	if (YEARSIZE(year) == 366) monthsize[1] = 29;
	while (dayno - *months >= 0) dayno -= *months++;
	pbuf->tm_mday = dayno + 1;
	pbuf->tm_mon = months - monthsize;
	monthsize[1] = 28;
	return pbuf;
}

#define FIRSTSUNDAY(t)	(((t)->tm_yday - (t)->tm_wday + 420) % 7)
#define SUNDAY(day, t)	((day) < 58 ? \
			  ((day) < FIRSTSUNDAY(t) ? FIRSTSUNDAY(t) :
static int
last_sunday(d, t)
	register int d;
	register struct tm *t;
{
	int first = FIRSTSUNDAY(t);

	if (d >= 58 && YEARSIZE(t->tm_year)) d++;
	if (d < first) return first;
	return d - (d - first) % 7;
}

extern struct tm *gmtime();

struct tm *
localtime(clock)
	long *clock;
{
	register struct tm *gmt;
	long cl;
	int begindst, enddst;
	extern int __daylight;
	extern long __timezone;

	tzset();
	cl = *clock - __timezone;
	gmt = gmtime(&cl);
	if (__daylight) {
		/* daylight saving time.
		   Unfortunately, rules differ for different countries.
		   Implemented here are heuristics that got it right
		   in Holland, over the last couple of years.
		   Of course, there is no algorithm. It is all
		   politics ...
		*/
		begindst = last_sunday(89, gmt); /* last Sun before Apr */
		enddst = last_sunday(272, gmt);  /* last Sun in Sep */
	    	if ((gmt->tm_yday>begindst ||
		     (gmt->tm_yday==begindst && gmt->tm_hour>=2)) &&
	    	    (gmt->tm_yday<enddst || 
		     (gmt->tm_yday==enddst && gmt->tm_hour<3))) {
			/* it all happens between 2 and 3 */
			cl += 1*60*60;
			gmt = gmtime(&cl);
			gmt->tm_isdst++;
		}
	}
	return gmt;
}
#ifdef BSD4_2
#else
#ifndef USG
#endif
#endif

#ifdef USG
long	timezone = -1 * 60;
int	daylight = 1;
char	*tzname[] = {"MET", "MDT",};
#endif

long __timezone = -1 * 60;
int __daylight = 1;
char *__tzname[] = {"MET", "MDT", };

tzset()
{
#ifdef BSD4_2
	struct timeval tval;
	struct timezone tzon;

	gettimeofday(&tval, &tzon);
	__timezone = tzon.tz_minuteswest * 60L;
	__daylight = tzon.tz_dsttime;
#else
#ifndef USG
	struct timeb time;

	ftime(&time);
	__timezone = time.timezone*60L;
	__daylight = time.dstflag;
#endif
#endif

	{
	extern char *getenv();
	register char *p = getenv("TZ");

	if (p && *p) {
		register int n = 0;
		int sign = 1;

		strncpy(__tzname[0], p, 3);
		p += 3;
		if (*(p += 3) == '-') {
			sign = -1;
			p++;
		}

		while(*p >= '0' && *p <= '9')
			n = 10 * n + (*p++ - '0');
		n *= sign;
		__timezone = ((long)(n * 60)) * 60;
		__daylight = (*p != '\0');
		strncpy(__tzname[1], p, 3);
	}
	}
#ifdef USG
	timezone = __timezone;
	daylight = __daylight;
	tzname[0] = __tzname[0];
	tzname[1] = __tzname[1];
#endif
}
