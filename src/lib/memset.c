/*  memset(3)
 *
 *  Author: Terrence W. Holm          Sep. 1988
 */


char *memset( vector, chr, count )
  char *vector;
  int   chr;
  int   count;

  {
  register char *memory = vector;

  while( --count >= 0 )
    *memory++ = chr;

  return( vector );
  }
