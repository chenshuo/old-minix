/* analyze - analyze relative performance of two string packages	*/
/*		Input is expected in the form produced by perf.		*/

#include <stdio.h>
long atol();

#define MAXLINE 80
#define OLD	0
#define NEW	1
#define TWOTABS	16
#define THREETABS 24

static void error_exit(rc, s)
int rc;
char *s;
{
  fprintf(stderr, "analyze: %s\n", s);
  exit(rc);
}

main(argc, argv)
int argc;
char *argv[];
{
  FILE *f[2];
  char line[2][MAXLINE];
  int loops[2];
  long time[2];
  int pct;
  int n;

  if (argc != 3 ||
     (f[OLD] = fopen(argv[1], "r")) == NULL ||
     (f[NEW] = fopen(argv[2], "r")) == NULL)
	error_exit(1, "Usage: analyze file1 file2");

  if (fgets(line[OLD], MAXLINE, f[OLD]) == NULL ||
      sscanf(line[OLD], "Loop count: %d,000", &loops[OLD]) != 1)
	error_exit(2, "no header in first file");
  if (fgets(line[NEW], MAXLINE, f[NEW]) == NULL ||
      sscanf(line[NEW], "Loop count: %d,000", &loops[NEW]) != 1)
	error_exit(2, "no header in second file");

  printf("Function\t\tNew time as percentage of old time\n");
  printf("---------------------\t-----------------------------------------\n");
  fgets(line[OLD], MAXLINE, f[OLD]);
  while (!feof(f[OLD])) {				/* header line */
	if (fgets(line[NEW], MAXLINE, f[NEW]) == NULL ||
	    strcmp(line[OLD], line[NEW]) != 0)
		error_exit(3, "synchronization failure");
	n = strlen(line[OLD]) - 1;
	printf("%.*s", n, line[OLD]);
	if (n < TWOTABS)
		putchar('\t');
	if (n < THREETABS)
		putchar('\t');
	fgets(line[OLD], MAXLINE, f[OLD]);
	while (!feof(f[OLD]) && line[OLD][0] == '\t') {	/* detail line */
		if (fgets(line[NEW], MAXLINE, f[NEW]) == NULL)
			error_exit(3, "synchronization failure");
		if ((n = strcspn(line[OLD], ":")) >= strlen(line[OLD]))
			error_exit(4, "input failure");
		if (strncmp(line[OLD], line[NEW], n + 1) != 0)
			error_exit(3, "synchronization failure");
		time[OLD] = atol(&line[OLD][n+2]);
		time[NEW] = atol(&line[NEW][n+2]);
		pct = ((100 * time[NEW] + time[OLD]/2) * loops[OLD]) /
		      ((      time[OLD]              ) * loops[NEW]);
		printf("%-10.*s %2d\t", n, &line[OLD][1], pct);
		fgets(line[OLD], MAXLINE, f[OLD]);
	}
	putchar('\n');
  }
  exit(0);
}
