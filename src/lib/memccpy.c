/*  memccpy(3)
 *
 *  Author: Terrence W. Holm          Sep. 1988
 */

#define  NULL  (char *) 0


char *memccpy( to, from, chr, count )
  char *to;
  char *from;
  int   chr;
  int   count;

  {
  while( --count >= 0 )
    if ( (*to++ = *from++) == chr )
	return( from );

  return( NULL );
  }
