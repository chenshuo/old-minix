/* ascii - list lines with/without ASCII chars	Author: Andy Tanenbaum */

#define BUFSIZE 30000

char buf[BUFSIZE];		/* input buffer */
char *next;			/* start of line */
char *limit;			/* last char of line */
int count;			/* # chars in buffer not yet processed */
int used;			/* how many chars used at start of buf */
int eof;			/* set when eof seen */
int nflag;			/* set if -n option given */
int exitstatus;			/* 0 if pure ASCII, 1 if junk seen */

main(argc, argv)
int argc;
char *argv[];
{
  int yes;
  char *p;

  if (argc > 3) usage();
  if (strcmp(argv[1], "-n") == 0) nflag++;

  if ((argc == 2 && nflag == 0) || argc == 3) {
	close(0);
	if (open(argv[argc-1], 0) < 0) {
		std_err("ascii: cannot open ");
		std_err(argv[1]);
		std_err("\n");
		exit(1);
	}
  }

  while(eof == 0) {
	yes = getline();
	if (nflag != yes) output();
	next = limit;
  }
  exit(exitstatus);
}

int getline()
{
  char *p, c;
  int asc = 1;

  if (count == 0) load();
  if (eof) exit(exitstatus);

  p = next;
  while (count > 0) {
	c = *p++;
	if (c & 0200) {asc = 0; exitstatus = 1;}
	count--;
	if (c == '\n') {
		limit = p;
		return(asc);
	}
	if (count == 0) {
		/* Move the residual characters to the bottom of buf */
		used = &buf[BUFSIZE] - next;
		copy(next, buf, used);
		load();
		p = &buf[used];
		used = 0;
		if (eof) return(asc);
	}
  }
}

load()
{
  count = read(0, &buf[used], BUFSIZE-used);
  if (count <= 0) eof = 1;
  next = buf;
}

output()
{
  write(1, next, limit-next);
}

usage()
{
  std_err("Usage: ascii [-n] file\n");
  exit(1);
}

copy(s,d,ct)
register char *s, *d;
int ct;
{
  while (ct--) *d++ = *s++;
}

