#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/times.h>

/* If you have old Minix headers, you may need to define the following.  */
/* You should be using the stdio package I posted (unpaid political ad). */
/* #define CLK_TCK 60 */
/* typedef long clock_t; */

#define INNERLOOPCOUNT 1000

#define TIME_EXECUTION(x, t) {			\
  times(&start);				\
  for (i = loop_count; --i >= 0; )		\
    for (j = INNERLOOPCOUNT; --j >= 0; )	\
      dummy = (int) x;				\
  times(&end);					\
  t = end.tms_utime - start.tms_utime;		\
  }

#define PRINT_TIME(x, args, note) {		\
  TIME_EXECUTION(x, elapsed_time);		\
  printf("\t%s: %d\n", note, elapsed_time - empty_function_time[args]); \
  }

static char ATOE[] = "ABCDE";
static char ATOZ[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static int empty_function(x, y, z)
int x, y, z;
{
  return x;
}

main()
{
  struct tms start, end;
  int loop_count;
  clock_t empty_function_time[4];
  long ns;
  clock_t elapsed_time;
  static char s1[31];
  static char s2[30];
  static char buf[1024];
  static char buf2[1024];
  register int i, j;
  int dummy;
  int a1, a2, a3;

/* --- Find a loop count that results in nontrivial function call times --- */
  empty_function_time[0] = 0;
  empty_function_time[1] = 0;
  for (loop_count = 6; empty_function_time[1] < CLK_TCK/10; loop_count += 2)
    TIME_EXECUTION(empty_function(a1), empty_function_time[1]);
  TIME_EXECUTION(empty_function(a1, a2), empty_function_time[2]);
  TIME_EXECUTION(empty_function(a1, a2, a3), empty_function_time[3]);
  ns = (1000000L * empty_function_time[2]) / (CLK_TCK * loop_count);
  printf("Loop count: %d,000\tTypical function call: %d.%1d us\n",
	loop_count, (int) ns/1000, (int) ((ns%1000) + 50)/100);

/* --- memcpy --- */
  printf("memcpy(s1, s2, n):\n");
  PRINT_TIME(memcpy(s1 + 1, s2, 4),   3, "[n=4]");
  PRINT_TIME(memcpy(s1, s2 + 1, 25),  3, "[n=25]");
  PRINT_TIME(memcpy(buf, buf2, 1024), 3, "[n=1024]");

/* --- strcpy --- */
  printf("strcpy(s1, s2):\n");
  PRINT_TIME(strcpy(s1, ATOE), 2, "[s2=ATOE]");
  PRINT_TIME(strcpy(s1, ATOZ), 2, "[s2=ATOZ]");

/* --- strncpy --- */
  printf("strncpy(s1, s2, n):\n");
  PRINT_TIME(strncpy(s1, ATOZ, 10), 3, "[s2=ATOZ,n=10]");

/* --- memcmp --- */
  printf("memcmp(buf, buf2, n):\n");
  memset(buf, 0, 1024);
  memset(buf2, 0, 1024);
  PRINT_TIME(memcmp(buf, buf2, 4),    3, "[n=4]");
  PRINT_TIME(memcmp(buf, buf2, 25),   3, "[n=25]");
  PRINT_TIME(memcmp(buf, buf2, 1024), 3, "[n=1024]");

/* --- strcmp --- */
  printf("strcmp(s1, s2):\n");
  strcpy(s1, ATOE);
  PRINT_TIME(strcmp(s1, ATOE), 2, "[2*ATOE]");
  strcpy(s1, ATOZ);
  PRINT_TIME(strcmp(s1 + 1, ATOZ + 1), 2, "[2*ATOZ]");	/* unaligned */

/* --- strncmp --- */
  printf("strncmp(s1, s2, n):\n");
  strcpy(s1, ATOZ);
  PRINT_TIME(strncmp(s1, ATOZ, 4),  3, "[n=4]");
  PRINT_TIME(strncmp(s1, ATOZ, 25), 3, "[n=25]");

/* --- memchr --- */
  printf("memchr(ATOZ, c, 25):\n");
  PRINT_TIME(memchr(ATOZ, 'E', 25), 3, "[c='E']");
  PRINT_TIME(memchr(ATOZ, 'Z', 25), 3, "[c='Z']");

/* --- strchr --- */
  printf("strchr(ATOZ, c):\n");
  PRINT_TIME(strchr(ATOZ, 'E'), 2, "[c='E']");
  PRINT_TIME(strchr(ATOZ, 'Z'), 2, "[c='Z']");

/* --- strpbrk --- */
  printf("strpbrk(\"word list\", s):\n");
  PRINT_TIME(strpbrk("word list", " "), 2, "[s=\" \"]");
  PRINT_TIME(strpbrk("word list", " \t\r\n"), 2, "[s=\" \\t\\r\\n\"]");

/* --- strrchr --- */
  printf("strrchr(ATOZ, c):\n");
  PRINT_TIME(strrchr(ATOZ, 'A'), 2, "[c='A']");
  PRINT_TIME(strrchr(ATOZ, 'E'), 2, "[c='E']");
  PRINT_TIME(strrchr(ATOZ, 'Z'), 2, "[c='Z']");

/* --- strstr --- */
  printf("strstr(ATOZ, s):\n");
  PRINT_TIME(strstr(ATOZ, "a"), 2, "[s=\"a\"]");
  PRINT_TIME(strstr(ATOZ, "y"), 2, "[s=\"y\"]");
  PRINT_TIME(strstr(ATOZ, "klmnop"), 2, "[s=\"klmnop\"]");

/* --- memset --- */
  printf("memset(buf, 0, n):\n");
  PRINT_TIME(memset(buf, 0, 4),    3, "[n=4]");
  PRINT_TIME(memset(buf, 0, 1024), 3, "[n=1024]");

/* --- strlen --- */
  printf("strlen(s):\n");
  PRINT_TIME(strlen(ATOE), 1, "[s=ATOE]");
  PRINT_TIME(strlen(ATOZ), 1, "[s=ATOZ]");

  exit(0);
}
