/*  memchr(3)
 *
 *  Author: Terrence W. Holm          Sep. 1988
 */

#define  NULL  (char *) 0


char *memchr( vector, chr, count )
  char *vector;
  int   chr;
  int   count;

  {
  while( --count >= 0 )
    if ( *vector++ == chr )
	return( vector - 1 );

  return( NULL );
  }
