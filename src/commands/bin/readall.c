/* readall - read a whole device fast		Author: Andy Tanenbaum */

#define BLK 30

char a[32000];

main(argc,argv)
int argc;
char *argv[];
{
  int fd;
  unsigned b=0;
  if (argc != 2) {printf("Usage: readall file\n"); exit(1);}
  fd = open(argv[1], 0);
  if (fd < 0) {printf("%s is not readable\n", argv[1]); exit(1);}

  while(1) {
	if (read(fd, a, 1024*BLK) == 0) {output(b); exit(0);}
	b++;
	if (b % 100 == 0) output(b);
  }
}

output(b)
unsigned b;
{
  printf("%D blocks read\n",(long)b * (long) BLK);
}
