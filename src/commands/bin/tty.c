/* tty.c - Return tty name		Author: Freeman P. Pascal IV */

#include <stdio.h>

char	*ttyname();

main(argc, argv)
int  argc;
char *argv[];
{
  char *tty_name;

  tty_name = ttyname( 0 );
  if(( argc == 2 ) && ( !strcmp(argv[1], "-s") ))
	/* do nothing - shhh! we're in silent mode */;
  else
	printf("%s\n", ((tty_name) ? tty_name : "not a tty"));
  exit((tty_name) ? 0 : 1 );
}

