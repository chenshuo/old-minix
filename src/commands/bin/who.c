/* who - tell who is currently logged in	Author: Andy Tanenbaum */

/* Who reads the file /usr/adm/wtmp and prints a list of who is curently
 * logged in.  The format of this file is a sequence of 20-character records,
 * as defined by struct wtmprec below.  There is an implicit assumption that
 * all terminal names are of the form ttyn, where n is a single decimal digit.
 */

#define SLOTS   10
#define WTMPSIZE 8
#define DIGIT    3

char *wtmpfile = "/usr/adm/wtmp";

struct wtmprec {
  char wt_line[WTMPSIZE];	/* tty name */
  char wt_name[WTMPSIZE];	/* user id */
  long wt_time;			/* time */
} wtmp;

struct wtmprec user[SLOTS];
extern char *ctime();

main()
{
  int fd;

  fd = open(wtmpfile, 0);
  if (fd < 0) {
	printf("The file %s cannot be opened.\n", wtmpfile);
	printf("To enable login accounting (required by who),");
	printf("create an empty file with this name.\n");
	exit(1);
  }

  readwtmp(fd);
  printwtmp();
}


readwtmp(fd)
int fd;
{
/* Read the /usr/adm/wtmp file and build up a log of current users. */

  int i, ttynr;

  while (read(fd, &wtmp, sizeof(wtmp)) == sizeof(wtmp)) {
	if (strcmp(wtmp.wt_line, "~") == 0) {
		/* This line means that the system was rebooted. */
		for (i = 0; i < SLOTS; i++) user[i].wt_line[0] = 0;
		continue;
	}
	ttynr = wtmp.wt_line[DIGIT] - '0';
	if (ttynr < 0 || ttynr >= SLOTS) continue;
	if (wtmp.wt_name[0] == 0) {
		user[ttynr].wt_line[0] = 0 ;
		continue;
	}
	user[ttynr] = wtmp;
  }
}

printwtmp()
{
  struct wtmprec *w;
  char *p;

  for (w = &user[0]; w < &user[SLOTS]; w++) {
	if (w->wt_line[0] == 0) continue;
	printf("%s	%s	", w->wt_name, w->wt_line);
	p = ctime(&w->wt_time);
	*(p+16) = 0;
	printf("%s\n", p);
  }
}
