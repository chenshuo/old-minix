/* clr - clear the screen		Author: Andy Tanenbaum */

/* Changed for termcap.       1988-Apr-2          efth         */


#include <stdio.h>

#define  TC_BUFFER  1024	/* Size of termcap(3) buffer	*/

extern char *getenv();
extern char *tgetstr();

char  buffer[ TC_BUFFER ];


main()
  {
  char *term;
  char  clear[ 30 ];
  char *p = &clear[0];

  if ( (term = getenv( "TERM" )) == NULL )
    Error( "$TERM not defined" );

  if ( tgetent( buffer, term ) != 1 )
    Error( "No termcap definition for $TERM" );

  if ( (tgetstr( "cl", &p )) == NULL )
    Error( "No clear (cl) entry for $TERM" );

  /*  Clear the screen  */

  printf( "%s", clear );

  exit(0);
  }



Error( str )
  char *str;

  {
  fprintf( stderr, "clr: %s\n", str );
  exit( 1 );
  }
