/* head - print the first few lines of a file	Author: Andy Tanenbaum */
/* change to use putc() instead of prints()    --  Dean Long 3/7/87    */


#include "stdio.h"

#define DEFAULT 10

char buff[BUFSIZ];

main(argc, argv)
int argc;
char *argv[];
{

  int n, k, nfiles;
  char *ptr;

  /* Check for flag.  Only flag is -n, to say how many lines to print. */
  setbuf(stdout, buff);
  k = 1;
  ptr = argv[1];
  n = DEFAULT;
  if (*ptr++ == '-') {
  	k++;
	n = atoi(ptr);
  	if (n <= 0) usage();
  }
  nfiles = argc - k;

  if (nfiles == 0) {
  	/* Print standard input only. */
  	do_file(n);
	fflush(stdout);
  	exit(0);
  }

  /* One or more files have been listed explicitly. */
  while (k < argc) {
  	fclose(stdin);
  	if (nfiles > 1) prints("==> %s <==\n", argv[k]);
  	if (fopen(argv[k], "r") == NULL) 
		prints("head: cannot open %s\n", argv[k]);
	else {
		do_file(n);
	 	fflush(stdout);
	}
  	k++;
  	if (k < argc) prints("\n");
  }
  exit(0);
}



do_file(n)
int n;
{
  int c;

  /* Print the first 'n' lines of a file. */
  while(n)
    switch (c = getc(stdin)) {
      case EOF :
        return;
      case '\n':
        --n;
      default  :
        putc((char)c, stdout);
    }
}


usage()
{
  std_err("Usage: head [-n] [file ...]\n");
  exit(1);
}

